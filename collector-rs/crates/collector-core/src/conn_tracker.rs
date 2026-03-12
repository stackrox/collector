use std::collections::HashMap;
use std::net::{IpAddr, Ipv4Addr};
use std::time::Duration;

use collector_types::address::{Endpoint, IpNetwork};
use collector_types::connection::{ConnStatus, Connection, L4Protocol, Role};

/// Tracks active/inactive network connections with afterglow, normalization, and filtering.
pub struct ConnTracker {
    connections: HashMap<Connection, ConnStatus>,
    ignored_ports: Vec<(L4Protocol, u16)>,
    ignored_networks: Vec<IpNetwork>,
    afterglow_period: Duration,
}

/// A delta entry representing a connection that was added or removed since last report.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ConnectionUpdate {
    Added(Connection),
    Removed(Connection),
}

impl ConnTracker {
    /// Creates a new tracker with the given afterglow period for suppressing transient removals.
    pub fn new(afterglow_period: Duration) -> Self {
        Self {
            connections: HashMap::new(),
            ignored_ports: Vec::new(),
            ignored_networks: Vec::new(),
            afterglow_period,
        }
    }

    /// Records a connection open or close event with its timestamp.
    pub fn update_connection(&mut self, conn: Connection, timestamp_us: u64, active: bool) {
        let status = ConnStatus::new(timestamp_us, active);
        self.connections
            .entry(conn)
            .and_modify(|s| *s = status)
            .or_insert(status);
    }

    /// Configures ports to exclude from reported connections.
    pub fn set_ignored_ports(&mut self, ports: Vec<(L4Protocol, u16)>) {
        self.ignored_ports = ports;
    }

    /// Configures CIDR networks to exclude from reported connections.
    pub fn set_ignored_networks(&mut self, networks: Vec<IpNetwork>) {
        self.ignored_networks = networks;
    }

    /// Updates the afterglow period used for delta computation.
    pub fn set_afterglow_period(&mut self, period: Duration) {
        self.afterglow_period = period;
    }

    /// Snapshots current connections, optionally normalizing endpoints and purging inactive entries.
    pub fn fetch_state(
        &mut self,
        normalize: bool,
        clear_inactive: bool,
    ) -> HashMap<Connection, ConnStatus> {
        let mut result = HashMap::new();
        let ignored_ports = &self.ignored_ports;
        let ignored_networks = &self.ignored_networks;

        self.connections.retain(|conn, status| {
            if !should_include_conn(conn, ignored_ports, ignored_networks) {
                return !clear_inactive;
            }

            let key = if normalize {
                normalize_conn(conn)
            } else {
                conn.clone()
            };

            result
                .entry(key)
                .and_modify(|existing: &mut ConnStatus| {
                    if status.timestamp_us() > existing.timestamp_us() {
                        *existing = *status;
                    }
                })
                .or_insert(*status);

            !clear_inactive || status.is_active()
        });

        result
    }

    /// Computes additions/removals between two state snapshots, respecting afterglow.
    pub fn compute_delta(
        &self,
        old: &HashMap<Connection, ConnStatus>,
        new: &HashMap<Connection, ConnStatus>,
        now_us: u64,
    ) -> Vec<ConnectionUpdate> {
        let mut updates = Vec::new();
        let afterglow_us = self.afterglow_period.as_micros() as u64;

        // Additions: in new but not old, or was inactive and now active (past afterglow)
        for (conn, new_status) in new {
            match old.get(conn) {
                None => {
                    if new_status.is_active() {
                        updates.push(ConnectionUpdate::Added(conn.clone()));
                    }
                }
                Some(old_status) => {
                    if new_status.is_active()
                        && !old_status.is_active()
                        && !in_afterglow(old_status, now_us, afterglow_us)
                    {
                        updates.push(ConnectionUpdate::Added(conn.clone()));
                    }
                }
            }
        }

        // Removals: was active in old, now gone or inactive (past afterglow)
        for (conn, old_status) in old {
            if !old_status.is_active() {
                continue;
            }
            match new.get(conn) {
                None => {
                    updates.push(ConnectionUpdate::Removed(conn.clone()));
                }
                Some(new_status) => {
                    if !new_status.is_active()
                        && !in_afterglow(new_status, now_us, afterglow_us)
                    {
                        updates.push(ConnectionUpdate::Removed(conn.clone()));
                    }
                }
            }
        }

        updates
    }

}

fn normalize_conn(conn: &Connection) -> Connection {
    let mut normalized = conn.clone();
    match conn.role {
        Role::Server => {
            normalized.remote = Endpoint {
                address: IpAddr::V4(Ipv4Addr::UNSPECIFIED),
                port: 0,
            };
        }
        Role::Client => {
            normalized.local = Endpoint {
                address: IpAddr::V4(Ipv4Addr::UNSPECIFIED),
                port: 0,
            };
        }
    }
    normalized
}

fn should_include_conn(
    conn: &Connection,
    ignored_ports: &[(L4Protocol, u16)],
    ignored_networks: &[IpNetwork],
) -> bool {
    for (proto, port) in ignored_ports {
        if conn.protocol == *proto
            && (conn.local.port == *port || conn.remote.port == *port)
        {
            return false;
        }
    }
    for net in ignored_networks {
        if net.contains(conn.remote.address) || net.contains(conn.local.address) {
            return false;
        }
    }
    true
}

fn in_afterglow(status: &ConnStatus, now_us: u64, afterglow_us: u64) -> bool {
    if afterglow_us == 0 {
        return false;
    }
    let elapsed = now_us.saturating_sub(status.timestamp_us());
    elapsed < afterglow_us
}

#[cfg(test)]
mod tests {
    use super::*;
    use collector_types::container::ContainerId;

    fn make_conn(local_port: u16, remote_port: u16, role: Role) -> Connection {
        Connection {
            container_id: ContainerId("test".into()),
            local: Endpoint {
                address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 1)),
                port: local_port,
            },
            remote: Endpoint {
                address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 2)),
                port: remote_port,
            },
            protocol: L4Protocol::Tcp,
            role,
        }
    }

    #[test]
    fn update_tracks_state() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        let conn = make_conn(12345, 80, Role::Client);

        tracker.update_connection(conn.clone(), 1000, true);
        let state = tracker.fetch_state(false, false);
        assert!(state.get(&conn).unwrap().is_active());
    }

    #[test]
    fn close_marks_inactive() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        let conn = make_conn(12345, 80, Role::Client);

        tracker.update_connection(conn.clone(), 1000, true);
        tracker.update_connection(conn.clone(), 2000, false);
        let state = tracker.fetch_state(false, false);
        assert!(!state.get(&conn).unwrap().is_active());
    }

    #[test]
    fn delta_detects_additions() {
        let tracker = ConnTracker::new(Duration::from_secs(0));
        let conn = make_conn(12345, 80, Role::Client);

        let old = HashMap::new();
        let mut new = HashMap::new();
        new.insert(conn.clone(), ConnStatus::new(1000, true));

        let delta = tracker.compute_delta(&old, &new, 2000);
        assert_eq!(delta, vec![ConnectionUpdate::Added(conn)]);
    }

    #[test]
    fn delta_detects_removals() {
        let tracker = ConnTracker::new(Duration::from_secs(0));
        let conn = make_conn(12345, 80, Role::Client);

        let mut old = HashMap::new();
        old.insert(conn.clone(), ConnStatus::new(1000, true));
        let new = HashMap::new();

        let delta = tracker.compute_delta(&old, &new, 2000);
        assert_eq!(delta, vec![ConnectionUpdate::Removed(conn)]);
    }

    #[test]
    fn afterglow_suppresses_rapid_changes() {
        let tracker = ConnTracker::new(Duration::from_secs(10));
        let conn = make_conn(12345, 80, Role::Client);

        let mut old = HashMap::new();
        old.insert(conn.clone(), ConnStatus::new(1_000_000, true));

        let mut new = HashMap::new();
        // Connection went inactive at timestamp 2s
        new.insert(conn.clone(), ConnStatus::new(2_000_000, false));

        // At 3s: still within afterglow (10s), should NOT report removal
        let delta = tracker.compute_delta(&old, &new, 3_000_000);
        assert!(delta.is_empty());

        // At 15s: past afterglow, SHOULD report removal
        let delta = tracker.compute_delta(&old, &new, 15_000_000);
        assert_eq!(delta, vec![ConnectionUpdate::Removed(conn)]);
    }

    #[test]
    fn normalization_aggregates_server_clients() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));

        // Two different clients connecting to the same server port
        let conn1 = Connection {
            container_id: ContainerId("server".into()),
            local: Endpoint {
                address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 1)),
                port: 80,
            },
            remote: Endpoint {
                address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 100)),
                port: 50000,
            },
            protocol: L4Protocol::Tcp,
            role: Role::Server,
        };
        let conn2 = Connection {
            container_id: ContainerId("server".into()),
            local: Endpoint {
                address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 1)),
                port: 80,
            },
            remote: Endpoint {
                address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 200)),
                port: 50001,
            },
            protocol: L4Protocol::Tcp,
            role: Role::Server,
        };

        tracker.update_connection(conn1, 1000, true);
        tracker.update_connection(conn2, 1000, true);

        let state = tracker.fetch_state(true, false);
        // Should aggregate into one normalized connection
        assert_eq!(state.len(), 1);
    }

    #[test]
    fn ignored_ports_filtered() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        tracker.set_ignored_ports(vec![(L4Protocol::Tcp, 22)]);

        let conn = make_conn(12345, 22, Role::Client);
        tracker.update_connection(conn, 1000, true);

        let state = tracker.fetch_state(false, false);
        assert!(state.is_empty());
    }

    #[test]
    fn ignored_networks_filtered() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        tracker.set_ignored_networks(vec![IpNetwork {
            address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 0)),
            prefix_len: 8,
        }]);

        let conn = make_conn(12345, 80, Role::Client);
        tracker.update_connection(conn, 1000, true);

        let state = tracker.fetch_state(false, false);
        assert!(state.is_empty());
    }

    #[test]
    fn clear_inactive_removes_old_entries() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        let conn = make_conn(12345, 80, Role::Client);

        tracker.update_connection(conn.clone(), 1000, true);
        tracker.update_connection(conn.clone(), 2000, false);

        // First fetch without clearing — inactive still present
        let state = tracker.fetch_state(false, false);
        assert_eq!(state.len(), 1);

        // Fetch with clear_inactive — should remove it
        let state = tracker.fetch_state(false, true);
        assert_eq!(state.len(), 1); // returned in snapshot
        // But tracker should now be empty
        let state = tracker.fetch_state(false, false);
        assert!(state.is_empty());
    }

    // --- Add/Remove lifecycle tests (ported from C++ TestAddRemove) ---

    #[test]
    fn add_two_connections_both_visible() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        let conn1 = make_conn(50000, 80, Role::Client);
        let conn2 = make_conn(9999, 80, Role::Server);

        tracker.update_connection(conn1.clone(), 1000, true);
        tracker.update_connection(conn2.clone(), 1000, true);

        let state = tracker.fetch_state(false, false);
        assert_eq!(state.len(), 2);
        assert!(state.get(&conn1).unwrap().is_active());
        assert!(state.get(&conn2).unwrap().is_active());
    }

    #[test]
    fn fetch_state_is_idempotent_without_clear() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        let conn = make_conn(12345, 80, Role::Client);
        tracker.update_connection(conn.clone(), 1000, true);

        let state1 = tracker.fetch_state(false, false);
        let state2 = tracker.fetch_state(false, false);
        assert_eq!(state1.len(), state2.len());
        assert_eq!(
            state1.get(&conn).unwrap().is_active(),
            state2.get(&conn).unwrap().is_active()
        );
    }

    #[test]
    fn remove_then_fetch_shows_inactive_then_cleared() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        let conn1 = make_conn(50000, 80, Role::Client);
        let conn2 = make_conn(9999, 80, Role::Server);

        tracker.update_connection(conn1.clone(), 1000, true);
        tracker.update_connection(conn2.clone(), 1000, true);
        tracker.update_connection(conn1.clone(), 2000, false);

        // First fetch: conn1 inactive, conn2 active
        let state = tracker.fetch_state(false, true);
        assert!(!state.get(&conn1).unwrap().is_active());
        assert!(state.get(&conn2).unwrap().is_active());

        // Second fetch with clear: conn1 gone (was inactive)
        let state = tracker.fetch_state(false, true);
        assert!(state.get(&conn1).is_none());
        assert!(state.get(&conn2).unwrap().is_active());
    }

    // --- Afterglow delta tests (ported from C++ TestComputeDeltaAfterglow*) ---

    #[test]
    fn delta_active_old_active_new_is_empty() {
        let tracker = ConnTracker::new(Duration::from_micros(50));
        let conn = make_conn(12345, 80, Role::Client);

        let mut old = HashMap::new();
        old.insert(conn.clone(), ConnStatus::new(990, true));
        let mut new = HashMap::new();
        new.insert(conn.clone(), ConnStatus::new(1990, true));

        let delta = tracker.compute_delta(&old, &new, 2000);
        assert!(delta.is_empty());
    }

    #[test]
    fn delta_active_old_inactive_unexpired_new_is_empty() {
        // Connection went inactive recently (within afterglow), no removal reported
        let tracker = ConnTracker::new(Duration::from_micros(50));
        let conn = make_conn(12345, 80, Role::Client);

        let mut old = HashMap::new();
        old.insert(conn.clone(), ConnStatus::new(990, true));
        let mut new = HashMap::new();
        new.insert(conn.clone(), ConnStatus::new(1990, false));

        // now=2000, inactive at 1990, afterglow=50us → still in afterglow
        let delta = tracker.compute_delta(&old, &new, 2000);
        assert!(delta.is_empty());
    }

    #[test]
    fn delta_active_old_inactive_expired_new_reports_removal() {
        // Connection went inactive and afterglow expired
        let tracker = ConnTracker::new(Duration::from_micros(50));
        let conn = make_conn(12345, 80, Role::Client);

        let mut old = HashMap::new();
        old.insert(conn.clone(), ConnStatus::new(990, true));
        let mut new = HashMap::new();
        new.insert(conn.clone(), ConnStatus::new(1900, false));

        // now=2000, inactive at 1900, afterglow=50us → expired (100us > 50us)
        let delta = tracker.compute_delta(&old, &new, 2000);
        assert_eq!(delta, vec![ConnectionUpdate::Removed(conn)]);
    }

    #[test]
    fn delta_inactive_expired_old_active_new_reports_addition() {
        // Connection was expired inactive, now active again
        let tracker = ConnTracker::new(Duration::from_micros(50));
        let conn = make_conn(12345, 80, Role::Client);

        let mut old = HashMap::new();
        old.insert(conn.clone(), ConnStatus::new(900, false));
        let mut new = HashMap::new();
        new.insert(conn.clone(), ConnStatus::new(1900, true));

        // old was inactive and expired → new active = addition
        let delta = tracker.compute_delta(&old, &new, 2000);
        assert_eq!(delta, vec![ConnectionUpdate::Added(conn)]);
    }

    #[test]
    fn delta_active_old_no_new_reports_removal() {
        let tracker = ConnTracker::new(Duration::from_micros(50));
        let conn = make_conn(12345, 80, Role::Client);

        let mut old = HashMap::new();
        old.insert(conn.clone(), ConnStatus::new(990, true));
        let new = HashMap::new();

        let delta = tracker.compute_delta(&old, &new, 2000);
        assert_eq!(delta, vec![ConnectionUpdate::Removed(conn)]);
    }

    #[test]
    fn delta_no_old_active_new_reports_addition() {
        let tracker = ConnTracker::new(Duration::from_micros(50));
        let conn = make_conn(12345, 80, Role::Client);

        let old = HashMap::new();
        let mut new = HashMap::new();
        new.insert(conn.clone(), ConnStatus::new(900, true));

        let delta = tracker.compute_delta(&old, &new, 2000);
        assert_eq!(delta, vec![ConnectionUpdate::Added(conn)]);
    }

    #[test]
    fn delta_no_old_inactive_new_no_addition() {
        // New connection that's already inactive — don't report it
        let tracker = ConnTracker::new(Duration::from_micros(50));
        let conn = make_conn(12345, 80, Role::Client);

        let old = HashMap::new();
        let mut new = HashMap::new();
        new.insert(conn.clone(), ConnStatus::new(1900, false));

        let delta = tracker.compute_delta(&old, &new, 2000);
        assert!(delta.is_empty());
    }

    #[test]
    fn delta_inactive_old_no_new_no_removal() {
        // Already-inactive connection disappearing — no removal needed
        let tracker = ConnTracker::new(Duration::from_micros(50));
        let conn = make_conn(12345, 80, Role::Client);

        let mut old = HashMap::new();
        old.insert(conn.clone(), ConnStatus::new(990, false));
        let new = HashMap::new();

        let delta = tracker.compute_delta(&old, &new, 2000);
        assert!(delta.is_empty());
    }

    #[test]
    fn delta_same_active_state_is_empty() {
        let tracker = ConnTracker::new(Duration::from_secs(20));
        let conn1 = make_conn(50000, 80, Role::Client);
        let conn2 = make_conn(9999, 80, Role::Server);

        let mut state = HashMap::new();
        state.insert(conn1.clone(), ConnStatus::new(750, true));
        state.insert(conn2.clone(), ConnStatus::new(750, true));

        let delta = tracker.compute_delta(&state, &state, 1000);
        assert!(delta.is_empty());
    }

    #[test]
    fn delta_empty_old_copies_all_active() {
        let tracker = ConnTracker::new(Duration::from_secs(20));
        let conn1 = make_conn(50000, 80, Role::Client);
        let conn2 = make_conn(9999, 80, Role::Server);

        let old = HashMap::new();
        let mut new = HashMap::new();
        new.insert(conn1.clone(), ConnStatus::new(750, true));
        new.insert(conn2.clone(), ConnStatus::new(750, true));

        let delta = tracker.compute_delta(&old, &new, 1000);
        assert_eq!(delta.len(), 2);
    }

    #[test]
    fn delta_zero_afterglow_reports_inactive_immediately() {
        let tracker = ConnTracker::new(Duration::from_secs(0));
        let conn = make_conn(12345, 80, Role::Client);

        let mut old = HashMap::new();
        old.insert(conn.clone(), ConnStatus::new(990, true));
        let mut new = HashMap::new();
        new.insert(conn.clone(), ConnStatus::new(1990, false));

        let delta = tracker.compute_delta(&old, &new, 2000);
        assert_eq!(delta, vec![ConnectionUpdate::Removed(conn)]);
    }

    // --- Normalization tests ---

    #[test]
    fn normalization_aggregates_client_sources() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));

        // Two connections from different local ports to the same remote
        let conn1 = Connection {
            container_id: ContainerId("client".into()),
            local: Endpoint {
                address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 1)),
                port: 50000,
            },
            remote: Endpoint {
                address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 100)),
                port: 80,
            },
            protocol: L4Protocol::Tcp,
            role: Role::Client,
        };
        let conn2 = Connection {
            container_id: ContainerId("client".into()),
            local: Endpoint {
                address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 1)),
                port: 50001,
            },
            remote: Endpoint {
                address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 100)),
                port: 80,
            },
            protocol: L4Protocol::Tcp,
            role: Role::Client,
        };

        tracker.update_connection(conn1, 1000, true);
        tracker.update_connection(conn2, 1000, true);

        let state = tracker.fetch_state(true, false);
        // Client-side normalization zeros local endpoint → both aggregate
        assert_eq!(state.len(), 1);
    }

    #[test]
    fn normalization_keeps_different_protocols_separate() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        let tcp_conn = make_conn(12345, 80, Role::Client);
        let mut udp_conn = tcp_conn.clone();
        udp_conn.protocol = L4Protocol::Udp;

        tracker.update_connection(tcp_conn, 1000, true);
        tracker.update_connection(udp_conn, 1000, true);

        let state = tracker.fetch_state(true, false);
        assert_eq!(state.len(), 2);
    }

    // --- Ignored ports / networks edge cases ---

    #[test]
    fn ignored_port_on_local_side() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        tracker.set_ignored_ports(vec![(L4Protocol::Tcp, 22)]);

        // local port is the ignored one
        let conn = make_conn(22, 50000, Role::Server);
        tracker.update_connection(conn, 1000, true);

        let state = tracker.fetch_state(false, false);
        assert!(state.is_empty());
    }

    #[test]
    fn ignored_port_different_protocol_not_filtered() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        tracker.set_ignored_ports(vec![(L4Protocol::Udp, 53)]);

        // TCP on port 53 should NOT be filtered
        let conn = make_conn(12345, 53, Role::Client);
        tracker.update_connection(conn.clone(), 1000, true);

        let state = tracker.fetch_state(false, false);
        assert_eq!(state.len(), 1);
    }

    #[test]
    fn multiple_ignored_networks() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        tracker.set_ignored_networks(vec![
            IpNetwork {
                address: IpAddr::V4(Ipv4Addr::new(169, 254, 0, 0)),
                prefix_len: 16,
            },
            IpNetwork {
                address: IpAddr::V4(Ipv4Addr::new(224, 0, 0, 0)),
                prefix_len: 4,
            },
        ]);

        // Link-local remote
        let mut conn1 = make_conn(12345, 80, Role::Client);
        conn1.remote.address = IpAddr::V4(Ipv4Addr::new(169, 254, 1, 1));
        tracker.update_connection(conn1, 1000, true);

        // Multicast remote
        let mut conn2 = make_conn(12345, 5353, Role::Client);
        conn2.remote.address = IpAddr::V4(Ipv4Addr::new(224, 0, 0, 251));
        tracker.update_connection(conn2, 1000, true);

        // Normal connection
        let conn3 = make_conn(12345, 80, Role::Client);
        tracker.update_connection(conn3.clone(), 1000, true);

        let state = tracker.fetch_state(false, false);
        assert_eq!(state.len(), 1);
        assert!(state.contains_key(&conn3));
    }

    #[test]
    fn ignored_network_on_local_side() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        tracker.set_ignored_networks(vec![IpNetwork {
            address: IpAddr::V4(Ipv4Addr::new(169, 254, 0, 0)),
            prefix_len: 16,
        }]);

        let mut conn = make_conn(12345, 80, Role::Client);
        conn.local.address = IpAddr::V4(Ipv4Addr::new(169, 254, 1, 1));
        tracker.update_connection(conn, 1000, true);

        let state = tracker.fetch_state(false, false);
        assert!(state.is_empty());
    }

    // --- IPv6 support ---

    #[test]
    fn ipv6_connections_tracked() {
        use std::net::Ipv6Addr;

        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        let conn = Connection {
            container_id: ContainerId("test".into()),
            local: Endpoint {
                address: IpAddr::V6(Ipv6Addr::new(0xfd00, 0, 0, 0, 0, 0, 0, 1)),
                port: 50000,
            },
            remote: Endpoint {
                address: IpAddr::V6(Ipv6Addr::new(0xfd00, 0, 0, 0, 0, 0, 0, 2)),
                port: 80,
            },
            protocol: L4Protocol::Tcp,
            role: Role::Client,
        };

        tracker.update_connection(conn.clone(), 1000, true);
        let state = tracker.fetch_state(false, false);
        assert!(state.get(&conn).unwrap().is_active());
    }

    // --- UDP connections ---

    #[test]
    fn udp_connections_tracked() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        let conn = Connection {
            container_id: ContainerId("test".into()),
            local: Endpoint {
                address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 1)),
                port: 12345,
            },
            remote: Endpoint {
                address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 2)),
                port: 53,
            },
            protocol: L4Protocol::Udp,
            role: Role::Client,
        };

        tracker.update_connection(conn.clone(), 1000, true);
        let state = tracker.fetch_state(false, false);
        assert!(state.get(&conn).unwrap().is_active());
    }

    // --- Update overwrites previous state ---

    #[test]
    fn update_overwrites_timestamp() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        let conn = make_conn(12345, 80, Role::Client);

        tracker.update_connection(conn.clone(), 1000, true);
        tracker.update_connection(conn.clone(), 5000, true);

        let state = tracker.fetch_state(false, false);
        assert_eq!(state.get(&conn).unwrap().timestamp_us(), 5000);
    }

    #[test]
    fn reopen_connection_after_close() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        let conn = make_conn(12345, 80, Role::Client);

        tracker.update_connection(conn.clone(), 1000, true);
        tracker.update_connection(conn.clone(), 2000, false);
        tracker.update_connection(conn.clone(), 3000, true);

        let state = tracker.fetch_state(false, false);
        assert!(state.get(&conn).unwrap().is_active());
        assert_eq!(state.get(&conn).unwrap().timestamp_us(), 3000);
    }
}
