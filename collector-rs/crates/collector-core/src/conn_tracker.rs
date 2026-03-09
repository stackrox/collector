use std::collections::HashMap;
use std::net::{IpAddr, Ipv4Addr};
use std::time::Duration;

use collector_types::address::{Endpoint, IpNetwork};
use collector_types::connection::{ConnStatus, Connection, L4Protocol, Role};

pub struct ConnTracker {
    connections: HashMap<Connection, ConnStatus>,
    ignored_ports: Vec<(L4Protocol, u16)>,
    ignored_networks: Vec<IpNetwork>,
    afterglow_period: Duration,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ConnectionUpdate {
    Added(Connection),
    Removed(Connection),
}

impl ConnTracker {
    pub fn new(afterglow_period: Duration) -> Self {
        Self {
            connections: HashMap::new(),
            ignored_ports: Vec::new(),
            ignored_networks: Vec::new(),
            afterglow_period,
        }
    }

    pub fn update_connection(&mut self, conn: Connection, timestamp_us: u64, active: bool) {
        let status = ConnStatus::new(timestamp_us, active);
        self.connections
            .entry(conn)
            .and_modify(|s| *s = status)
            .or_insert(status);
    }

    pub fn set_ignored_ports(&mut self, ports: Vec<(L4Protocol, u16)>) {
        self.ignored_ports = ports;
    }

    pub fn set_ignored_networks(&mut self, networks: Vec<IpNetwork>) {
        self.ignored_networks = networks;
    }

    pub fn set_afterglow_period(&mut self, period: Duration) {
        self.afterglow_period = period;
    }

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
}
