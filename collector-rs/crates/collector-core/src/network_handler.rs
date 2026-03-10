use std::net::{IpAddr, Ipv4Addr, Ipv6Addr};
use std::sync::Mutex;

use collector_bpf::events::ConnectEvent;
use collector_types::address::Endpoint;
use collector_types::connection::{Connection, L4Protocol, Role};
use collector_types::container::ContainerId;
use tokio::sync::mpsc;
use tokio_util::sync::CancellationToken;
use tracing::debug;

use crate::conn_tracker::ConnTracker;
use crate::container_id::{extract_container_id, extract_container_id_from_proc};
use crate::event_reader::NetworkEvent;
use crate::metrics;

const AF_INET: u16 = 2;
const AF_INET6: u16 = 10;
const IPPROTO_TCP: u8 = 6;

/// Consumes network events from BPF and updates the shared ConnTracker state.
pub async fn run_network_handler(
    mut rx: mpsc::Receiver<NetworkEvent>,
    conn_tracker: &Mutex<ConnTracker>,
    cancel: CancellationToken,
) {
    loop {
        let event = tokio::select! {
            _ = cancel.cancelled() => break,
            evt = rx.recv() => match evt {
                Some(e) => e,
                None => break,
            }
        };

        if let Some((conn, active)) = parse_network_event(&event) {
            let now_us = std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap_or_default()
                .as_micros() as u64;
            let mut tracker = conn_tracker.lock().unwrap();
            tracker.update_connection(conn, now_us, active);
            metrics::inc_counter("network_events_processed");
        }
    }
    debug!("network handler stopped");
}

fn parse_network_event(event: &NetworkEvent) -> Option<(Connection, bool)> {
    let (evt, active, role) = match event {
        NetworkEvent::Connect(e) => (e, true, Role::Client),
        NetworkEvent::Accept(e) => (e, true, Role::Server),
        NetworkEvent::Close(e) => (e, false, infer_close_role(e)),
        NetworkEvent::Listen(e) => (e, true, Role::Server),
    };

    // Skip failed syscalls (connect returns negative on error)
    if active && evt.retval < 0 {
        return None;
    }

    let cgroup = evt.cgroup_str().trim_end_matches('\0');
    let container_id_owned;
    let container_id = match extract_container_id(cgroup) {
        Some(id) => id,
        None => {
            // Fallback: try /proc/{pid}/cgroup when BPF cgroup walk returns empty
            container_id_owned = extract_container_id_from_proc(evt.header.pid)?;
            &container_id_owned
        }
    };

    let (local_addr, remote_addr) = parse_addresses(evt)?;

    // Skip loopback
    if local_addr.is_loopback() || remote_addr.is_loopback() {
        return None;
    }

    let protocol = if evt.protocol == IPPROTO_TCP {
        L4Protocol::Tcp
    } else {
        L4Protocol::Udp
    };

    let conn = Connection {
        container_id: ContainerId(container_id.to_string()),
        local: Endpoint {
            address: local_addr,
            port: evt.sport,
        },
        remote: Endpoint {
            address: remote_addr,
            port: evt.dport,
        },
        protocol,
        role,
    };

    Some((conn, active))
}

fn infer_close_role(evt: &ConnectEvent) -> Role {
    // If local port is well-known (< 1024) it's likely a server
    if evt.sport < 1024 && evt.dport >= 1024 {
        Role::Server
    } else {
        Role::Client
    }
}

fn parse_addresses(evt: &ConnectEvent) -> Option<(IpAddr, IpAddr)> {
    match evt.family {
        AF_INET => {
            let mut src = [0u8; 4];
            let mut dst = [0u8; 4];
            src.copy_from_slice(&evt.saddr[..4]);
            dst.copy_from_slice(&evt.daddr[..4]);
            Some((
                IpAddr::V4(Ipv4Addr::from(src)),
                IpAddr::V4(Ipv4Addr::from(dst)),
            ))
        }
        AF_INET6 => {
            let src: [u8; 16] = evt.saddr;
            let dst: [u8; 16] = evt.daddr;
            Some((
                IpAddr::V6(Ipv6Addr::from(src)),
                IpAddr::V6(Ipv6Addr::from(dst)),
            ))
        }
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::mem;

    fn make_connect_event(
        family: u16,
        sport: u16,
        dport: u16,
        cgroup: &str,
    ) -> ConnectEvent {
        let mut evt: ConnectEvent = unsafe { mem::zeroed() };
        evt.header.event_type = 10;
        evt.family = family;
        evt.sport = sport;
        evt.dport = dport;
        evt.protocol = IPPROTO_TCP;
        evt.retval = 0;

        // Set IPv4 addresses: 10.0.0.1 -> 10.0.0.2
        evt.saddr[0] = 10;
        evt.saddr[3] = 1;
        evt.daddr[0] = 10;
        evt.daddr[3] = 2;

        let cg_bytes = cgroup.as_bytes();
        let len = cg_bytes.len().min(evt.cgroup.len());
        evt.cgroup[..len].copy_from_slice(&cg_bytes[..len]);
        evt.cgroup_len = len as u32;

        evt
    }

    const TEST_CGROUP: &str =
        "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";

    #[test]
    fn connect_event_parsed() {
        let evt = make_connect_event(AF_INET, 50000, 80, TEST_CGROUP);
        let net_evt = NetworkEvent::Connect(evt);
        let (conn, active) = parse_network_event(&net_evt).unwrap();
        assert!(active);
        assert_eq!(conn.role, Role::Client);
        assert_eq!(conn.remote.port, 80);
        assert_eq!(conn.protocol, L4Protocol::Tcp);
    }

    #[test]
    fn accept_event_parsed() {
        let evt = make_connect_event(AF_INET, 80, 50000, TEST_CGROUP);
        let net_evt = NetworkEvent::Accept(evt);
        let (conn, active) = parse_network_event(&net_evt).unwrap();
        assert!(active);
        assert_eq!(conn.role, Role::Server);
    }

    #[test]
    fn close_event_parsed() {
        let evt = make_connect_event(AF_INET, 50000, 80, TEST_CGROUP);
        let net_evt = NetworkEvent::Close(evt);
        let (conn, active) = parse_network_event(&net_evt).unwrap();
        assert!(!active);
    }

    #[test]
    fn host_process_filtered() {
        let evt = make_connect_event(AF_INET, 50000, 80, "");
        let net_evt = NetworkEvent::Connect(evt);
        assert!(parse_network_event(&net_evt).is_none());
    }

    #[test]
    fn loopback_filtered() {
        let mut evt = make_connect_event(AF_INET, 50000, 80, TEST_CGROUP);
        evt.daddr = [0; 16];
        evt.daddr[0] = 127;
        evt.daddr[3] = 1;
        let net_evt = NetworkEvent::Connect(evt);
        assert!(parse_network_event(&net_evt).is_none());
    }

    #[test]
    fn failed_connect_filtered() {
        let mut evt = make_connect_event(AF_INET, 50000, 80, TEST_CGROUP);
        evt.retval = -1;
        let net_evt = NetworkEvent::Connect(evt);
        assert!(parse_network_event(&net_evt).is_none());
    }
}
