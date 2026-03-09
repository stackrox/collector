use std::sync::Mutex;

use collector_bpf::events::ExecEvent;
use collector_types::container::ContainerId;
use collector_types::process::ProcessInfo;
use prost_types::Timestamp;
use tokio::sync::mpsc;
use tokio_util::sync::CancellationToken;
use tracing::{debug, warn};

use crate::container_id::extract_container_id;
use crate::event_reader::ProcessEvent;
use crate::metrics;
use crate::process_table::ProcessTable;
use crate::proto::sensor;
use crate::rate_limit::RateLimitCache;

#[async_trait::async_trait]
pub trait SignalSender: Send + Sync {
    async fn send(&self, msg: sensor::SignalStreamMessage) -> anyhow::Result<()>;
}

const FILTERED_COMM: &[&str] = &["runc", "conmon", "runc:[2:INIT]"];

pub async fn run_process_handler(
    mut rx: mpsc::Receiver<ProcessEvent>,
    sender: Box<dyn SignalSender>,
    process_table: &Mutex<ProcessTable>,
    rate_limiter: &Mutex<RateLimitCache>,
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

        match event {
            ProcessEvent::Exec(evt) => {
                handle_exec(&evt, &sender, process_table, rate_limiter).await;
            }
            ProcessEvent::Exit(evt) => {
                let mut table = process_table.lock().unwrap();
                table.remove(evt.header.pid);
            }
        }
    }
    debug!("process handler stopped");
}

async fn handle_exec(
    evt: &ExecEvent,
    sender: &Box<dyn SignalSender>,
    process_table: &Mutex<ProcessTable>,
    rate_limiter: &Mutex<RateLimitCache>,
) {
    let cgroup = evt.cgroup_str().trim_end_matches('\0');
    let container_id = match extract_container_id(cgroup) {
        Some(id) => id,
        None => return, // host process, skip
    };

    let comm = evt.comm_str().trim_end_matches('\0');

    if FILTERED_COMM.iter().any(|&f| comm == f) {
        return;
    }

    let filename = evt.filename_str().trim_end_matches('\0');
    let args = parse_args(evt.args_bytes());

    let container_id_obj = ContainerId(container_id.to_string());

    let info = ProcessInfo {
        pid: evt.header.pid,
        uid: evt.header.uid,
        gid: evt.header.gid,
        exe_path: filename.to_string(),
        args: args.clone(),
        container_id: container_id_obj.clone(),
    };

    let lineage = {
        let mut table = process_table.lock().unwrap();
        table.upsert(info, evt.ppid);
        table.lineage(evt.header.pid, &container_id_obj)
    };

    // Rate limit check
    let rate_key = format!(
        "{}:{}:{}:{}",
        container_id,
        comm,
        &args[..args.len().min(256)],
        filename
    );
    {
        let mut limiter = rate_limiter.lock().unwrap();
        if !limiter.allow(&rate_key) {
            metrics::inc_counter("process_rate_limited");
            return;
        }
    }

    let timestamp_ns = evt.header.timestamp_ns;
    let secs = (timestamp_ns / 1_000_000_000) as i64;
    let nanos = (timestamp_ns % 1_000_000_000) as i32;

    let name = sanitize_utf8(comm);
    let exec_file_path = sanitize_utf8(filename);
    let sanitized_args = sanitize_utf8(&args);

    let lineage_info: Vec<crate::proto::storage::process_signal::LineageInfo> = lineage
        .into_iter()
        .map(|l| crate::proto::storage::process_signal::LineageInfo {
            parent_uid: l.parent_uid,
            parent_exec_file_path: l.parent_exe_path,
        })
        .collect();

    let signal_msg = sensor::SignalStreamMessage {
        msg: Some(sensor::signal_stream_message::Msg::Signal(
            crate::proto::v1::Signal {
                signal: Some(crate::proto::v1::signal::Signal::ProcessSignal(
                    crate::proto::storage::ProcessSignal {
                        id: uuid::Uuid::new_v4().to_string(),
                        container_id: container_id.to_string(),
                        time: Some(Timestamp { seconds: secs, nanos }),
                        name: if !name.is_empty() {
                            name.clone()
                        } else {
                            exec_file_path.clone()
                        },
                        args: sanitized_args,
                        exec_file_path: if !exec_file_path.is_empty() {
                            exec_file_path
                        } else {
                            name
                        },
                        pid: evt.header.pid,
                        uid: evt.header.uid,
                        gid: evt.header.gid,
                        scraped: false,
                        lineage_info,
                        ..Default::default()
                    },
                )),
            },
        )),
    };

    if let Err(e) = sender.send(signal_msg).await {
        warn!("failed to send process signal: {}", e);
        metrics::inc_counter("process_send_failures");
    } else {
        metrics::inc_counter("process_signals_sent");
    }
}

fn parse_args(args_bytes: &[u8]) -> String {
    args_bytes
        .split(|&b| b == 0)
        .filter(|s| !s.is_empty())
        .map(|s| String::from_utf8_lossy(s))
        .collect::<Vec<_>>()
        .join(" ")
}

fn sanitize_utf8(s: &str) -> String {
    s.chars()
        .map(|c| if c.is_control() && c != '\n' && c != '\t' { '?' } else { c })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::mem;
    use std::sync::Arc;
    use tokio::sync::Mutex as TokioMutex;

    struct MockSender {
        messages: Arc<TokioMutex<Vec<sensor::SignalStreamMessage>>>,
    }

    #[async_trait::async_trait]
    impl SignalSender for MockSender {
        async fn send(&self, msg: sensor::SignalStreamMessage) -> anyhow::Result<()> {
            self.messages.lock().await.push(msg);
            Ok(())
        }
    }

    fn make_exec_event(pid: u32, comm: &str, filename: &str, cgroup: &str) -> ExecEvent {
        let mut evt: ExecEvent = unsafe { mem::zeroed() };
        evt.header.event_type = 1;
        evt.header.pid = pid;
        evt.header.uid = 1000;
        evt.header.gid = 1000;
        evt.header.timestamp_ns = 1_000_000_000;
        evt.ppid = 1;

        let comm_bytes = comm.as_bytes();
        let len = comm_bytes.len().min(evt.comm.len());
        evt.comm[..len].copy_from_slice(&comm_bytes[..len]);
        evt.comm_len = len as u32;

        let fn_bytes = filename.as_bytes();
        let len = fn_bytes.len().min(evt.filename.len());
        evt.filename[..len].copy_from_slice(&fn_bytes[..len]);
        evt.filename_len = len as u32;

        let cg_bytes = cgroup.as_bytes();
        let len = cg_bytes.len().min(evt.cgroup.len());
        evt.cgroup[..len].copy_from_slice(&cg_bytes[..len]);
        evt.cgroup_len = len as u32;

        evt
    }

    #[tokio::test]
    async fn host_process_is_filtered() {
        let messages = Arc::new(TokioMutex::new(Vec::new()));
        let sender: Box<dyn SignalSender> = Box::new(MockSender {
            messages: messages.clone(),
        });
        let table = Mutex::new(ProcessTable::new());
        let limiter = Mutex::new(RateLimitCache::new());

        let evt = make_exec_event(100, "ls", "/usr/bin/ls", "");
        handle_exec(&evt, &sender, &table, &limiter).await;

        assert!(messages.lock().await.is_empty());
    }

    #[tokio::test]
    async fn runc_process_is_filtered() {
        let messages = Arc::new(TokioMutex::new(Vec::new()));
        let sender: Box<dyn SignalSender> = Box::new(MockSender {
            messages: messages.clone(),
        });
        let table = Mutex::new(ProcessTable::new());
        let limiter = Mutex::new(RateLimitCache::new());

        let evt = make_exec_event(
            100,
            "runc",
            "/usr/bin/runc",
            "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789",
        );
        handle_exec(&evt, &sender, &table, &limiter).await;

        assert!(messages.lock().await.is_empty());
    }

    #[tokio::test]
    async fn container_process_is_sent() {
        let messages = Arc::new(TokioMutex::new(Vec::new()));
        let sender: Box<dyn SignalSender> = Box::new(MockSender {
            messages: messages.clone(),
        });
        let table = Mutex::new(ProcessTable::new());
        let limiter = Mutex::new(RateLimitCache::new());

        let evt = make_exec_event(
            100,
            "nginx",
            "/usr/sbin/nginx",
            "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789",
        );
        handle_exec(&evt, &sender, &table, &limiter).await;

        let msgs = messages.lock().await;
        assert_eq!(msgs.len(), 1);
    }
}
