use std::process::Command;
use std::time::Duration;

use anyhow::{bail, Result};
use collector_bpf::events::EventType;
use collector_bpf::EventSource;
use tracing::info;

pub fn verify_bpf(source: &mut dyn EventSource) -> Result<()> {
    info!("running BPF self-check: executing /bin/true");

    // Execute a known binary to generate an exec event
    Command::new("/bin/true")
        .output()
        .map_err(|e| anyhow::anyhow!("failed to execute /bin/true: {}", e))?;

    // Poll for the exec event
    let deadline = std::time::Instant::now() + Duration::from_secs(5);
    while std::time::Instant::now() < deadline {
        if let Some(event) = source.next_event(Duration::from_millis(100)) {
            let event_type = match &event {
                collector_bpf::events::RawEvent::Exec(_) => Some(EventType::ProcessExec),
                collector_bpf::events::RawEvent::Exit(_) => Some(EventType::ProcessExit),
                _ => None,
            };
            if event_type == Some(EventType::ProcessExec) {
                info!("BPF self-check passed: exec event received");
                return Ok(());
            }
        }
    }

    bail!("BPF self-check failed: no exec event received within 5 seconds")
}
