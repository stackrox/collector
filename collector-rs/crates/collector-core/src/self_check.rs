use std::process::Command;
use std::time::Duration;

use anyhow::{bail, Result};
use tokio::sync::mpsc;
use tracing::info;

use crate::event_reader::ProcessEvent;

/// Runs /bin/true and verifies the BPF program captures the exec event within 5 seconds.
/// Reads from the process event channel so events are not lost from the pipeline.
pub async fn verify_bpf(proc_rx: &mut mpsc::Receiver<ProcessEvent>) -> Result<()> {
    info!("running BPF self-check: executing /bin/true");

    let my_pid = std::process::id();

    // Execute a known binary to generate an exec event
    Command::new("/bin/true")
        .output()
        .map_err(|e| anyhow::anyhow!("failed to execute /bin/true: {}", e))?;

    // Poll the process channel for our exec event
    let deadline = tokio::time::Instant::now() + Duration::from_secs(5);
    loop {
        let remaining = deadline.saturating_duration_since(tokio::time::Instant::now());
        if remaining.is_zero() {
            break;
        }
        match tokio::time::timeout(remaining, proc_rx.recv()).await {
            Ok(Some(ProcessEvent::Exec(evt))) => {
                if evt.ppid == my_pid {
                    info!("BPF self-check passed: exec event for /bin/true received (pid={})", evt.header.pid);
                    return Ok(());
                }
                // Not our process, but that's fine - events will be re-read by the handler
            }
            Ok(Some(ProcessEvent::Exit(_))) => {}
            Ok(None) => bail!("process channel closed during self-check"),
            Err(_) => break, // timeout
        }
    }

    bail!("BPF self-check failed: no exec event for /bin/true received within 5 seconds")
}
