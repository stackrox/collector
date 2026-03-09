use std::collections::HashMap;
use std::net::IpAddr;
use std::sync::{Arc, Mutex};
use std::time::Duration;

use anyhow::{Context, Result};
use prost_types::Timestamp;
use tokio::sync::mpsc;
use tokio_util::sync::CancellationToken;
use tonic::transport::Channel;
use tracing::{debug, info, warn};

use collector_types::connection::{ConnStatus, Connection, L4Protocol, Role};

use crate::conn_tracker::{ConnTracker, ConnectionUpdate};
use crate::metrics;
use crate::proto::sensor;
use crate::proto::sensor::network_connection_info_service_client::NetworkConnectionInfoServiceClient;
use crate::proto::storage;

/// Periodically computes connection deltas and streams them to Sensor via gRPC.
pub async fn run_network_client(
    endpoint: String,
    hostname: String,
    instance_id: String,
    conn_tracker: Arc<Mutex<ConnTracker>>,
    scrape_interval: Duration,
    cancel: CancellationToken,
) {
    loop {
        if cancel.is_cancelled() {
            break;
        }

        info!("connecting to sensor network service at {}", endpoint);

        match connect_and_report(
            &endpoint,
            &hostname,
            &instance_id,
            &conn_tracker,
            scrape_interval,
            &cancel,
        )
        .await
        {
            Ok(()) => {
                debug!("network client stream ended normally");
                break;
            }
            Err(e) => {
                warn!("network client error: {}, reconnecting in 5s", e);
                tokio::select! {
                    _ = cancel.cancelled() => break,
                    _ = tokio::time::sleep(Duration::from_secs(5)) => {}
                }
            }
        }
    }
    debug!("network client loop stopped");
}

async fn connect_and_report(
    endpoint: &str,
    hostname: &str,
    instance_id: &str,
    conn_tracker: &Arc<Mutex<ConnTracker>>,
    scrape_interval: Duration,
    cancel: &CancellationToken,
) -> Result<()> {
    let channel = Channel::from_shared(endpoint.to_string())
        .context("invalid endpoint")?
        .connect()
        .await
        .context("failed to connect")?;

    let mut client = NetworkConnectionInfoServiceClient::new(channel);

    let (stream_tx, stream_rx) = mpsc::channel::<sensor::NetworkConnectionInfoMessage>(256);

    // Send registration message
    let register_msg = sensor::NetworkConnectionInfoMessage {
        msg: Some(
            sensor::network_connection_info_message::Msg::Register(
                sensor::CollectorRegisterRequest {
                    hostname: hostname.to_string(),
                    instance_id: instance_id.to_string(),
                },
            ),
        ),
    };
    stream_tx
        .send(register_msg)
        .await
        .context("failed to send register")?;

    let stream = tokio_stream::wrappers::ReceiverStream::new(stream_rx);
    let response = client
        .push_network_connection_info(stream)
        .await
        .context("failed to open network stream")?;

    // Handle inbound control messages
    let mut response_stream = response.into_inner();
    let conn_tracker_for_control = Arc::clone(conn_tracker);
    let cancel_clone = cancel.clone();
    tokio::spawn(async move {
        loop {
            tokio::select! {
                _ = cancel_clone.cancelled() => break,
                msg = response_stream.message() => {
                    match msg {
                        Ok(Some(control)) => {
                            handle_control_message(&conn_tracker_for_control, &control);
                        }
                        Ok(None) => break,
                        Err(e) => {
                            debug!("network response stream error: {}", e);
                            break;
                        }
                    }
                }
            }
        }
    });

    // Periodic delta reporting
    let mut prev_state: HashMap<Connection, ConnStatus> = HashMap::new();
    let mut interval = tokio::time::interval(scrape_interval);

    loop {
        tokio::select! {
            _ = cancel.cancelled() => return Ok(()),
            _ = interval.tick() => {
                let now_us = std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .unwrap_or_default()
                    .as_micros() as u64;

                let (new_state, updates) = {
                    let mut tracker = conn_tracker.lock().unwrap();
                    let new_state = tracker.fetch_state(true, true);
                    let updates = tracker.compute_delta(&prev_state, &new_state, now_us);
                    (new_state, updates)
                };

                if !updates.is_empty() {
                    let msg = build_network_info_message(&updates, now_us);
                    if stream_tx.send(msg).await.is_err() {
                        return Err(anyhow::anyhow!("network stream closed"));
                    }
                    metrics::set_counter("network_connections_reported", updates.len() as f64);
                }

                prev_state = new_state;
            }
        }
    }
}

fn handle_control_message(
    conn_tracker: &Mutex<ConnTracker>,
    control: &sensor::NetworkFlowsControlMessage,
) {
    let mut tracker = conn_tracker.lock().unwrap();

    if let Some(ref networks) = control.ip_networks {
        let mut ignored = Vec::new();
        let ipv4_data = &networks.ipv4_networks;
        for chunk in ipv4_data.chunks_exact(5) {
            let addr = IpAddr::V4(std::net::Ipv4Addr::new(chunk[0], chunk[1], chunk[2], chunk[3]));
            let prefix_len = chunk[4];
            ignored.push(collector_types::address::IpNetwork {
                address: addr,
                prefix_len,
            });
        }
        let ipv6_data = &networks.ipv6_networks;
        for chunk in ipv6_data.chunks_exact(17) {
            let mut octets = [0u8; 16];
            octets.copy_from_slice(&chunk[..16]);
            let addr = IpAddr::V6(std::net::Ipv6Addr::from(octets));
            let prefix_len = chunk[16];
            ignored.push(collector_types::address::IpNetwork {
                address: addr,
                prefix_len,
            });
        }
        tracker.set_ignored_networks(ignored);
    }

    debug!("applied network control message");
}

fn build_network_info_message(
    updates: &[ConnectionUpdate],
    now_us: u64,
) -> sensor::NetworkConnectionInfoMessage {
    let now_secs = (now_us / 1_000_000) as i64;
    let now_nanos = ((now_us % 1_000_000) * 1000) as i32;

    let connections: Vec<sensor::NetworkConnection> = updates
        .iter()
        .map(|update| {
            let (conn, close_ts) = match update {
                ConnectionUpdate::Added(c) => (c, None),
                ConnectionUpdate::Removed(c) => (
                    c,
                    Some(Timestamp {
                        seconds: now_secs,
                        nanos: now_nanos,
                    }),
                ),
            };

            let socket_family = match conn.local.address {
                IpAddr::V4(_) => sensor::SocketFamily::Ipv4,
                IpAddr::V6(_) => sensor::SocketFamily::Ipv6,
            };

            let protocol = match conn.protocol {
                L4Protocol::Tcp => storage::L4Protocol::Tcp,
                L4Protocol::Udp => storage::L4Protocol::Udp,
            };

            let role = match conn.role {
                Role::Client => sensor::ClientServerRole::RoleClient,
                Role::Server => sensor::ClientServerRole::RoleServer,
            };

            sensor::NetworkConnection {
                socket_family: socket_family as i32,
                local_address: Some(addr_to_proto(&conn.local)),
                remote_address: Some(addr_to_proto(&conn.remote)),
                protocol: protocol as i32,
                role: role as i32,
                container_id: conn.container_id.0.clone(),
                close_timestamp: close_ts,
            }
        })
        .collect();

    let info = sensor::NetworkConnectionInfo {
        updated_connections: connections,
        time: Some(Timestamp {
            seconds: now_secs,
            nanos: now_nanos,
        }),
        ..Default::default()
    };

    sensor::NetworkConnectionInfoMessage {
        msg: Some(sensor::network_connection_info_message::Msg::Info(info)),
    }
}

fn addr_to_proto(endpoint: &collector_types::address::Endpoint) -> sensor::NetworkAddress {
    let address_data = match endpoint.address {
        IpAddr::V4(v4) => v4.octets().to_vec(),
        IpAddr::V6(v6) => v6.octets().to_vec(),
    };

    sensor::NetworkAddress {
        address_data,
        port: endpoint.port as u32,
        ip_network: Vec::new(),
    }
}
