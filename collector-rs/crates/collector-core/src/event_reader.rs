use std::thread::{self, JoinHandle};
use std::time::Duration;

use collector_bpf::events::{ConnectEvent, ExecEvent, ExitEvent, RawEvent};
use collector_bpf::EventSource;
use tokio::sync::mpsc;
use tokio_util::sync::CancellationToken;
use tracing::{debug, warn};

use crate::metrics;

/// Typed wrapper for process-related BPF events (exec or exit).
#[derive(Debug)]
pub enum ProcessEvent {
    Exec(ExecEvent),
    Exit(ExitEvent),
}

/// Typed wrapper for network-related BPF events (connect/accept/close/listen).
#[derive(Debug)]
pub enum NetworkEvent {
    Connect(ConnectEvent),
    Accept(ConnectEvent),
    Close(ConnectEvent),
    Listen(ConnectEvent),
}

const POLL_TIMEOUT: Duration = Duration::from_millis(100);

/// Spawns a dedicated OS thread that polls the BPF event source and routes events to channels.
pub fn spawn_event_reader(
    mut source: Box<dyn EventSource>,
    process_tx: mpsc::Sender<ProcessEvent>,
    network_tx: mpsc::Sender<NetworkEvent>,
    cancel: CancellationToken,
) -> JoinHandle<()> {
    thread::Builder::new()
        .name("event-reader".into())
        .spawn(move || {
            debug!("event reader started");
            while !cancel.is_cancelled() {
                let Some(event) = source.next_event(POLL_TIMEOUT) else {
                    continue;
                };
                metrics::inc_counter("events_total");
                match event {
                    RawEvent::Exec(evt) => {
                        metrics::COLLECTOR_EVENTS
                            .with_label_values(&["exec"])
                            .inc();
                        if process_tx.blocking_send(ProcessEvent::Exec(evt)).is_err() {
                            warn!("process channel closed, stopping event reader");
                            break;
                        }
                    }
                    RawEvent::Exit(evt) => {
                        metrics::COLLECTOR_EVENTS
                            .with_label_values(&["exit"])
                            .inc();
                        if process_tx.blocking_send(ProcessEvent::Exit(evt)).is_err() {
                            warn!("process channel closed, stopping event reader");
                            break;
                        }
                    }
                    RawEvent::Connect(evt) => {
                        metrics::COLLECTOR_EVENTS
                            .with_label_values(&["connect"])
                            .inc();
                        if network_tx
                            .blocking_send(NetworkEvent::Connect(evt))
                            .is_err()
                        {
                            warn!("network channel closed, stopping event reader");
                            break;
                        }
                    }
                    RawEvent::Accept(evt) => {
                        metrics::COLLECTOR_EVENTS
                            .with_label_values(&["accept"])
                            .inc();
                        if network_tx
                            .blocking_send(NetworkEvent::Accept(evt))
                            .is_err()
                        {
                            warn!("network channel closed, stopping event reader");
                            break;
                        }
                    }
                    RawEvent::Close(evt) => {
                        metrics::COLLECTOR_EVENTS
                            .with_label_values(&["close"])
                            .inc();
                        if network_tx
                            .blocking_send(NetworkEvent::Close(evt))
                            .is_err()
                        {
                            warn!("network channel closed, stopping event reader");
                            break;
                        }
                    }
                    RawEvent::Listen(evt) => {
                        metrics::COLLECTOR_EVENTS
                            .with_label_values(&["listen"])
                            .inc();
                        if network_tx
                            .blocking_send(NetworkEvent::Listen(evt))
                            .is_err()
                        {
                            warn!("network channel closed, stopping event reader");
                            break;
                        }
                    }
                }
            }
            debug!("event reader stopped");
        })
        .expect("failed to spawn event reader thread")
}

#[cfg(test)]
mod tests {
    use super::*;
    use collector_bpf::MockEventSource;
    use std::mem;

    fn make_exec_event(pid: u32) -> ExecEvent {
        let mut evt: ExecEvent = unsafe { mem::zeroed() };
        evt.header.event_type = 1; // ProcessExec
        evt.header.pid = pid;
        evt
    }

    fn make_connect_event() -> ConnectEvent {
        let mut evt: ConnectEvent = unsafe { mem::zeroed() };
        evt.header.event_type = 10; // SocketConnect
        evt
    }

    #[tokio::test]
    async fn routes_events_to_correct_channels() {
        let events = vec![
            RawEvent::Exec(make_exec_event(100)),
            RawEvent::Connect(make_connect_event()),
            RawEvent::Exit(unsafe { mem::zeroed() }),
        ];
        let source = MockEventSource::new(events);

        let (proc_tx, mut proc_rx) = mpsc::channel(16);
        let (net_tx, mut net_rx) = mpsc::channel(16);
        let cancel = CancellationToken::new();

        let handle = spawn_event_reader(Box::new(source), proc_tx, net_tx, cancel.clone());

        // Wait briefly for events to be processed
        tokio::time::sleep(Duration::from_millis(300)).await;
        cancel.cancel();
        handle.join().unwrap();

        // Should have received 2 process events (exec + exit) and 1 network event
        let mut proc_count = 0;
        while proc_rx.try_recv().is_ok() {
            proc_count += 1;
        }
        assert_eq!(proc_count, 2);

        let mut net_count = 0;
        while net_rx.try_recv().is_ok() {
            net_count += 1;
        }
        assert_eq!(net_count, 1);
    }
}
