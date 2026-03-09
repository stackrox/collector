# Collector Rust Conversion Plan

## Table of Contents

1. [Goals and Non-Goals](#1-goals-and-non-goals)
2. [Architecture Overview](#2-architecture-overview)
3. [Crate Structure](#3-crate-structure)
4. [Phase 1: Foundation Types and BPF Programs](#phase-1-foundation-types-and-bpf-programs)
5. [Phase 2: Event Pipeline](#phase-2-event-pipeline)
6. [Phase 3: Process Signal Handling](#phase-3-process-signal-handling)
7. [Phase 4: Network Signal Handling](#phase-4-network-signal-handling)
8. [Phase 5: gRPC Communication](#phase-5-grpc-communication)
9. [Phase 6: Configuration and Management](#phase-6-configuration-and-management)
10. [Phase 7: Integration and Packaging](#phase-7-integration-and-packaging)
11. [BPF Program Design](#bpf-program-design)
12. [Testing Plan](#testing-plan)
13. [Migration Strategy](#13-migration-strategy)
14. [Key Design Decisions](#14-key-design-decisions)
15. [Risk Register](#15-risk-register)

---

## 1. Goals and Non-Goals

### Goals

- **Replace falcosecurity-libs** with custom BPF programs using `libbpf-rs` and `aya`, giving us full control over the kernel-userspace interface
- **Memory safety** via Rust's ownership model, eliminating the classes of bugs that come from C++ (use-after-free, data races, buffer overflows)
- **Simpler threading** using Rust's `tokio` async runtime instead of hand-rolled `StoppableThread` + mutex + condition variable patterns
- **Testability** through trait-based dependency injection, making every component unit-testable without kernel interaction
- **Wire compatibility** with Sensor - the protobuf messages and gRPC services must remain identical
- **Feature parity** with the current C++ collector

### Non-Goals

- Changing the Sensor-side protocol or requiring Sensor modifications
- Supporting kernel module or legacy eBPF drivers (modern CO-RE BPF only)
- Replicating the sinsp thread table / fd table in full generality - we only need what collector actually uses
- Keeping any C++ code in the final binary

---

## 2. Architecture Overview

### Current C++ Architecture (What We're Replacing)

```
Kernel BPF → libscap → libsinsp → system_inspector::Service → Handlers → gRPC → Sensor
```

The falcosecurity-libs stack (libscap + libsinsp) provides: ring buffer management, event parsing, thread table maintenance, fd table maintenance, container metadata via plugin, and a filter/extraction system. The collector only uses a small fraction of this.

### New Rust Architecture

```
Kernel BPF → ring_buf → EventReader → typed events → channels → Handlers → gRPC → Sensor
                                          ↓
                                    ProcessTable
                                    (maintained by
                                     BPF + userspace)
```

**Key structural changes:**

1. **No intermediary library** - BPF programs send structured events directly to userspace via ring buffers. No libscap/libsinsp parsing layer.
2. **Channel-based pipeline** instead of callback/handler chain. Events flow through `tokio::sync::mpsc` channels.
3. **Async-first** - gRPC, config watching, network reporting all use `tokio` tasks instead of OS threads with condition variables.
4. **Typed events from BPF** - BPF programs emit well-defined Rust structs (shared via `#[repr(C)]`), not opaque byte buffers that need a parameter table to decode.

### Data Flow

```
                    ┌──────────────────────────┐
                    │     BPF Programs          │
                    │  (exec, connect, close)   │
                    └──────────┬───────────────┘
                               │ ring_buf
                               ▼
                    ┌──────────────────────────┐
                    │     EventReader           │
                    │  (polls ring buffer,      │
                    │   deserializes events)    │
                    └──────────┬───────────────┘
                               │ RawEvent enum
                    ┌──────────┴───────────────┐
                    │                          │
                    ▼                          ▼
          ┌─────────────────┐      ┌──────────────────────┐
          │ ProcessHandler  │      │ NetworkHandler        │
          │ (enriches with  │      │ (extracts connection, │
          │  container_id,  │      │  updates tracker)     │
          │  lineage, args) │      │                       │
          └────────┬────────┘      └──────────┬───────────┘
                   │                          │
                   ▼                          ▼
          ┌─────────────────┐      ┌──────────────────────┐
          │ SignalClient    │      │ NetworkClient         │
          │ (gRPC stream)   │      │ (periodic delta push) │
          └─────────────────┘      └──────────────────────┘
```

---

## 3. Crate Structure

```
collector-rs/
├── Cargo.toml                  # Workspace root
├── crates/
│   ├── collector-bpf/          # BPF programs (C code compiled via libbpf-cargo)
│   │   ├── src/
│   │   │   ├── bpf/
│   │   │   │   ├── collector.bpf.c     # All BPF programs in one object
│   │   │   │   └── vmlinux.h           # Generated kernel types
│   │   │   ├── lib.rs                  # Skeleton loader + typed wrappers
│   │   │   └── events.rs              # Shared event structs (#[repr(C)])
│   │   └── build.rs                   # libbpf-cargo build
│   │
│   ├── collector-types/        # Shared types (no heavy dependencies)
│   │   └── src/
│   │       ├── lib.rs
│   │       ├── address.rs             # Address, Endpoint, IPNet
│   │       ├── connection.rs          # Connection, ConnStatus, L4Proto
│   │       ├── process.rs             # ProcessInfo, LineageInfo
│   │       └── container.rs           # ContainerId, container metadata
│   │
│   ├── collector-core/         # Main application logic
│   │   └── src/
│   │       ├── lib.rs
│   │       ├── config.rs              # Configuration (clap + serde)
│   │       ├── event_reader.rs        # BPF ring buffer consumer
│   │       ├── process_table.rs       # PID → ProcessInfo cache
│   │       ├── container_id.rs        # Cgroup → container ID resolution
│   │       ├── process_handler.rs     # Process event → ProcessSignal
│   │       ├── network_handler.rs     # Network event → ConnectionTracker update
│   │       ├── conn_tracker.rs        # Connection tracking + delta + afterglow
│   │       ├── rate_limit.rs          # Token bucket rate limiter
│   │       ├── signal_client.rs       # gRPC signal stream
│   │       ├── network_client.rs      # gRPC network flow stream
│   │       ├── metrics.rs             # Prometheus metrics
│   │       ├── health.rs              # HTTP health/management server
│   │       └── self_check.rs          # Driver validation
│   │
│   └── collector-bin/          # Binary entry point
│       └── src/
│           └── main.rs
│
└── proto/                      # Protobuf definitions (unchanged)
    ├── api/v1/
    ├── internalapi/sensor/
    └── storage/
```

### Why This Structure

- **`collector-bpf`**: Isolated because BPF compilation has special build requirements (`libbpf-cargo`, C compiler). Also makes it possible to mock the BPF layer entirely in tests.
- **`collector-types`**: Zero-dependency types crate. Used by both BPF (for `#[repr(C)]` shared structs) and core logic. Keeps compile times fast.
- **`collector-core`**: All business logic. Every component takes trait objects for its dependencies, making unit testing straightforward.
- **`collector-bin`**: Thin `main()` that wires everything together. Integration tests live here.

---

## Phase 1: Foundation Types and BPF Programs

### 1.1 Shared Event Types (`collector-types/src/`)

Define the types that cross the BPF-userspace boundary and are used throughout the application.

```rust
// crates/collector-types/src/address.rs

use std::net::{IpAddr, Ipv4Addr, Ipv6Addr};

/// Network address with port. Used for both local and remote endpoints.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Endpoint {
    pub address: IpAddr,
    pub port: u16,
}

/// CIDR network for IP aggregation and filtering.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct IpNetwork {
    pub address: IpAddr,
    pub prefix_len: u8,
}

impl IpNetwork {
    pub fn contains(&self, addr: IpAddr) -> bool { /* mask comparison */ }
}

/// Well-known private networks for classification.
pub fn private_networks() -> &'static [IpNetwork] {
    &[
        IpNetwork { address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 0)), prefix_len: 8 },
        IpNetwork { address: IpAddr::V4(Ipv4Addr::new(172, 16, 0, 0)), prefix_len: 12 },
        IpNetwork { address: IpAddr::V4(Ipv4Addr::new(192, 168, 0, 0)), prefix_len: 16 },
        // ... IPv6 equivalents
    ]
}
```

```rust
// crates/collector-types/src/connection.rs

use crate::address::Endpoint;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum L4Protocol {
    Tcp,
    Udp,
    Unknown,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Role {
    Client,
    Server,
    Unknown,
}

/// A network connection observed in a container.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Connection {
    pub container_id: ContainerId,
    pub local: Endpoint,
    pub remote: Endpoint,
    pub protocol: L4Protocol,
    pub role: Role,
}

/// Packed connection status: active flag + timestamp.
/// Uses a single u64 for cache efficiency (same as C++ version).
#[derive(Debug, Clone, Copy)]
pub struct ConnStatus(u64);

impl ConnStatus {
    const ACTIVE_BIT: u64 = 1 << 63;

    pub fn new(timestamp_us: u64, active: bool) -> Self {
        let ts = timestamp_us & !Self::ACTIVE_BIT;
        Self(if active { ts | Self::ACTIVE_BIT } else { ts })
    }

    pub fn is_active(self) -> bool { self.0 & Self::ACTIVE_BIT != 0 }
    pub fn timestamp_us(self) -> u64 { self.0 & !Self::ACTIVE_BIT }
}
```

```rust
// crates/collector-types/src/process.rs

use crate::container::ContainerId;

/// Process information extracted from BPF events.
#[derive(Debug, Clone)]
pub struct ProcessInfo {
    pub pid: u32,
    pub tid: u32,
    pub ppid: u32,
    pub uid: u32,
    pub gid: u32,
    pub comm: String,
    pub exe_path: String,
    pub args: Vec<String>,
    pub cwd: String,
    pub container_id: ContainerId,
    pub cgroup: String,
}

#[derive(Debug, Clone)]
pub struct LineageInfo {
    pub parent_exec_file_path: String,
    pub parent_uid: u32,
}
```

### 1.2 BPF Event Structs (Shared Between C and Rust)

```rust
// crates/collector-bpf/src/events.rs
// These structs are #[repr(C)] so they match the BPF-side C definitions.

/// Maximum bytes for strings sent from BPF.
pub const MAX_FILENAME_LEN: usize = 256;
pub const MAX_ARGS_LEN: usize = 1024;
pub const MAX_CGROUP_LEN: usize = 256;

/// Event types sent from BPF to userspace.
#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EventType {
    ProcessExec = 1,
    ProcessExit = 2,
    ProcessFork = 3,
    SocketConnect = 10,
    SocketAccept = 11,
    SocketClose = 12,
    SocketListen = 13,
}

/// Common header for all BPF events.
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct EventHeader {
    pub event_type: u32,
    pub timestamp_ns: u64,
    pub pid: u32,
    pub tid: u32,
    pub uid: u32,
    pub gid: u32,
}

/// Process exec event - sent on successful execve return.
#[repr(C)]
pub struct ExecEvent {
    pub header: EventHeader,
    pub ppid: u32,
    pub filename_len: u32,
    pub filename: [u8; MAX_FILENAME_LEN],
    pub args_len: u32,
    pub args: [u8; MAX_ARGS_LEN],       // null-separated argument list
    pub comm: [u8; 16],                  // TASK_COMM_LEN
    pub cgroup_len: u32,
    pub cgroup: [u8; MAX_CGROUP_LEN],
}

/// Network connection event - sent on connect/accept/close exit.
#[repr(C)]
pub struct ConnectEvent {
    pub header: EventHeader,
    pub socket_family: u16,              // AF_INET or AF_INET6
    pub protocol: u16,                   // IPPROTO_TCP or IPPROTO_UDP
    pub src_addr: [u8; 16],              // IPv4 in first 4 bytes, or full IPv6
    pub dst_addr: [u8; 16],
    pub src_port: u16,
    pub dst_port: u16,
    pub retval: i32,                     // syscall return value
    pub cgroup_len: u32,
    pub cgroup: [u8; MAX_CGROUP_LEN],
}

/// Process exit event.
#[repr(C)]
pub struct ExitEvent {
    pub header: EventHeader,
    pub exit_code: i32,
}
```

### 1.3 BPF Programs

See [Section 11: BPF Program Design](#bpf-program-design) for the full BPF C code.

### 1.4 BPF Skeleton Loader

```rust
// crates/collector-bpf/src/lib.rs

use libbpf_rs::{MapCore, ObjectBuilder, RingBufferBuilder};

mod events;
pub use events::*;

// Generated by libbpf-cargo build.rs
include!(concat!(env!("OUT_DIR"), "/collector.bpf.skel.rs"));

/// High-level wrapper around the BPF skeleton.
pub struct BpfLoader {
    skel: CollectorSkel<'static>,
    ring_buf: libbpf_rs::RingBuffer<'static>,
}

/// Trait for receiving events from BPF. Makes testing possible
/// without actual BPF programs.
pub trait EventSource: Send {
    /// Block until the next event is available or timeout.
    /// Returns None on timeout, Some on event.
    fn next_event(&mut self, timeout_ms: i32) -> Option<RawEvent>;
}

/// Parsed event from BPF ring buffer.
pub enum RawEvent {
    Exec(ExecEvent),
    Exit(ExitEvent),
    Connect(ConnectEvent),
    Accept(ConnectEvent),
    Close(ConnectEvent),
    Listen(ConnectEvent),
}
```

---

## Phase 2: Event Pipeline

### 2.1 Container ID Resolution (`collector-core/src/container_id.rs`)

Replace the falcosecurity-libs container plugin with direct cgroup parsing. The container ID is extracted from the cgroup path, which the BPF program reads from the task struct.

```rust
/// Extract container ID from a cgroup path.
///
/// Handles common runtimes:
///   - Docker: /docker/<id>/...
///   - CRI-O:  /crio-<id>.scope
///   - containerd: /cri-containerd-<id>.scope
///   - systemd: /.../docker-<id>.scope
pub fn extract_container_id(cgroup: &str) -> Option<&str> {
    // Try each pattern, return first 12 chars of the 64-char hex ID.
    for segment in cgroup.rsplit('/') {
        if let Some(id) = try_extract_id(segment) {
            return Some(&id[..12.min(id.len())]);
        }
    }
    None
}

fn try_extract_id(segment: &str) -> Option<&str> {
    // Pattern: bare 64-char hex (Docker)
    if segment.len() == 64 && segment.chars().all(|c| c.is_ascii_hexdigit()) {
        return Some(segment);
    }
    // Pattern: "docker-<id>.scope" or "crio-<id>.scope" or "cri-containerd-<id>.scope"
    for prefix in &["docker-", "crio-", "cri-containerd-"] {
        if let Some(rest) = segment.strip_prefix(prefix) {
            if let Some(id) = rest.strip_suffix(".scope") {
                if id.len() == 64 && id.chars().all(|c| c.is_ascii_hexdigit()) {
                    return Some(id);
                }
            }
        }
    }
    None
}
```

**Design improvement over C++**: The C++ version relies on the container plugin (a shared library loaded at runtime) to provide `container.id`. This is fragile - plugin loading order matters, and the plugin API is unstable across falcosecurity-libs versions. Direct cgroup parsing is simpler, has no external dependencies, and is trivially testable.

### 2.2 Process Table (`collector-core/src/process_table.rs`)

Replace sinsp's thread manager with a focused process cache.

```rust
use std::collections::HashMap;
use std::time::{Duration, Instant};
use collector_types::process::{ProcessInfo, LineageInfo};

/// Maintains a cache of known processes for lineage lookups
/// and existing-process enumeration.
///
/// Design improvements over sinsp's thread_manager:
/// - Only stores what collector needs (no fd table, no full thread info)
/// - LRU eviction instead of time-based purging
/// - No mutex needed - owned by the single event-processing task
pub struct ProcessTable {
    processes: HashMap<u32, ProcessEntry>,
    max_size: usize,
}

struct ProcessEntry {
    info: ProcessInfo,
    last_seen: Instant,
}

impl ProcessTable {
    pub fn new(max_size: usize) -> Self {
        Self {
            processes: HashMap::with_capacity(max_size),
            max_size,
        }
    }

    /// Insert or update a process. Returns the old entry if it existed.
    pub fn upsert(&mut self, info: ProcessInfo) -> Option<ProcessInfo> {
        self.evict_if_full();
        self.processes.insert(info.pid, ProcessEntry {
            info: info.clone(),
            last_seen: Instant::now(),
        }).map(|e| e.info)
    }

    /// Remove a process (on exit event).
    pub fn remove(&mut self, pid: u32) -> Option<ProcessInfo> {
        self.processes.remove(&pid).map(|e| e.info)
    }

    /// Look up a process by PID.
    pub fn get(&self, pid: u32) -> Option<&ProcessInfo> {
        self.processes.get(&pid).map(|e| &e.info)
    }

    /// Build process lineage by walking parent PIDs.
    /// Stops at container boundary or after max_depth ancestors.
    pub fn lineage(&self, pid: u32, container_id: &str, max_depth: usize) -> Vec<LineageInfo> {
        let mut result = Vec::new();
        let mut current_pid = pid;
        let mut last_exe = String::new();

        for _ in 0..max_depth {
            let parent = match self.get(current_pid).and_then(|p| self.get(p.ppid)) {
                Some(p) => p,
                None => break,
            };

            // Stop at container boundary
            if parent.container_id.as_str() != container_id {
                break;
            }

            // Collapse consecutive identical exepaths
            if parent.exe_path != last_exe {
                result.push(LineageInfo {
                    parent_exec_file_path: parent.exe_path.clone(),
                    parent_uid: parent.uid,
                });
                last_exe = parent.exe_path.clone();
            }

            current_pid = parent.pid;
        }

        result
    }

    /// Iterate all known processes (for SendExistingProcesses on gRPC reconnect).
    pub fn iter(&self) -> impl Iterator<Item = &ProcessInfo> {
        self.processes.values().map(|e| &e.info)
    }

    fn evict_if_full(&mut self) {
        if self.processes.len() >= self.max_size {
            // Evict oldest entry
            if let Some(&pid) = self.processes.iter()
                .min_by_key(|(_, e)| e.last_seen)
                .map(|(pid, _)| pid)
            {
                self.processes.remove(&pid);
            }
        }
    }
}
```

**Design improvement**: The C++ thread manager is a general-purpose data structure shared across all of libsinsp and accessed through a mutex. Our ProcessTable is single-owner (no mutex needed), stores only what we need, and uses LRU eviction instead of periodic sweeps.

### 2.3 Event Reader (`collector-core/src/event_reader.rs`)

Consumes BPF ring buffer events and dispatches them.

```rust
use collector_bpf::{EventSource, RawEvent};
use tokio::sync::mpsc;
use tokio_util::sync::CancellationToken;

/// Reads events from BPF and sends typed events to handler channels.
/// Runs on a dedicated OS thread (BPF polling is blocking).
pub fn spawn_event_reader(
    mut source: Box<dyn EventSource>,
    process_tx: mpsc::Sender<ProcessEvent>,
    network_tx: mpsc::Sender<NetworkEvent>,
    cancel: CancellationToken,
) -> std::thread::JoinHandle<()> {
    std::thread::spawn(move || {
        while !cancel.is_cancelled() {
            let event = match source.next_event(100) { // 100ms timeout
                Some(e) => e,
                None => continue,
            };

            match event {
                RawEvent::Exec(e) => {
                    let _ = process_tx.blocking_send(ProcessEvent::Exec(e));
                }
                RawEvent::Exit(e) => {
                    let _ = process_tx.blocking_send(ProcessEvent::Exit(e));
                }
                RawEvent::Connect(e) => {
                    let _ = network_tx.blocking_send(NetworkEvent::Connect(e));
                }
                RawEvent::Accept(e) => {
                    let _ = network_tx.blocking_send(NetworkEvent::Accept(e));
                }
                RawEvent::Close(e) => {
                    let _ = network_tx.blocking_send(NetworkEvent::Close(e));
                }
                RawEvent::Listen(e) => {
                    let _ = network_tx.blocking_send(NetworkEvent::Listen(e));
                }
            }
        }
    })
}
```

**Design improvement**: The C++ version uses a single main thread that blocks on `inspector_->next()` and synchronously dispatches to handlers in sequence. Our approach separates the BPF reader thread from handler processing, and uses separate channels for process and network events so they can be processed independently. This also eliminates the `libsinsp_mutex_` - the BPF reader is the only thing touching the ring buffer, and handlers own their own state.

---

## Phase 3: Process Signal Handling

### 3.1 Process Handler (`collector-core/src/process_handler.rs`)

```rust
use tokio::sync::mpsc;
use collector_types::process::ProcessInfo;
use crate::container_id::extract_container_id;
use crate::process_table::ProcessTable;
use crate::rate_limit::RateLimitCache;
use crate::signal_client::SignalSender;

pub enum ProcessEvent {
    Exec(collector_bpf::ExecEvent),
    Exit(collector_bpf::ExitEvent),
}

/// Processes exec/exit events, maintains the process table,
/// formats protobuf signals, and sends them to Sensor.
pub async fn run_process_handler(
    mut rx: mpsc::Receiver<ProcessEvent>,
    sender: Box<dyn SignalSender>,
    cancel: CancellationToken,
) {
    let mut process_table = ProcessTable::new(32_768);
    let mut rate_limiter = RateLimitCache::new(10, Duration::from_secs(1800));
    let mut metrics = ProcessMetrics::default();

    loop {
        tokio::select! {
            _ = cancel.cancelled() => break,
            event = rx.recv() => {
                let event = match event {
                    Some(e) => e,
                    None => break,
                };
                match event {
                    ProcessEvent::Exec(exec) => {
                        handle_exec(&exec, &mut process_table, &mut rate_limiter,
                                    &sender, &mut metrics).await;
                    }
                    ProcessEvent::Exit(exit) => {
                        process_table.remove(exit.header.pid);
                    }
                }
            }
        }
    }
}

async fn handle_exec(
    exec: &ExecEvent,
    table: &mut ProcessTable,
    rate_limiter: &mut RateLimitCache,
    sender: &dyn SignalSender,
    metrics: &mut ProcessMetrics,
) {
    let info = parse_exec_event(exec);

    // Skip host processes (no container ID)
    if info.container_id.is_empty() {
        return;
    }

    // Skip container runtime internals
    if is_container_runtime(&info) {
        return;
    }

    let lineage = table.lineage(info.pid, info.container_id.as_str(), 10);
    table.upsert(info.clone());

    // Rate limit check
    let key = rate_limit_key(&info);
    if !rate_limiter.allow(&key) {
        metrics.rate_limited += 1;
        return;
    }

    // Build and send protobuf
    let signal = build_process_signal(&info, &lineage);
    match sender.send_process_signal(signal).await {
        Ok(()) => metrics.sent += 1,
        Err(e) => {
            tracing::warn!("Failed to send process signal: {e}");
            metrics.send_failures += 1;
        }
    }
}

fn rate_limit_key(info: &ProcessInfo) -> String {
    let args_prefix: String = info.args.join(" ").chars().take(256).collect();
    format!("{}:{}:{}:{}", info.container_id, info.comm, args_prefix, info.exe_path)
}

fn is_container_runtime(info: &ProcessInfo) -> bool {
    matches!(info.comm.as_str(), "runc" | "runc:[" | "conmon")
}
```

**Design improvements:**
- No mutex on the process table (single-owner in one async task)
- Rate limiting key uses proper Rust string handling instead of C-style char truncation
- Container ID filtering is a simple string check instead of a compiled sinsp filter
- `SignalSender` trait allows unit testing without gRPC

### 3.2 Rate Limiter (`collector-core/src/rate_limit.rs`)

```rust
use std::collections::HashMap;
use std::time::{Duration, Instant};

/// Token bucket rate limiter keyed by string.
pub struct RateLimitCache {
    buckets: HashMap<String, TokenBucket>,
    burst: u32,
    refill_interval: Duration,
    max_entries: usize,
}

struct TokenBucket {
    tokens: u32,
    last_refill: Instant,
}

impl RateLimitCache {
    pub fn new(burst: u32, refill_interval: Duration) -> Self {
        Self {
            buckets: HashMap::new(),
            burst,
            refill_interval,
            max_entries: 65_536,
        }
    }

    /// Returns true if the event should be allowed through.
    pub fn allow(&mut self, key: &str) -> bool {
        let now = Instant::now();
        let burst = self.burst;
        let interval = self.refill_interval;

        let bucket = self.buckets.entry(key.to_string()).or_insert_with(|| {
            TokenBucket { tokens: burst, last_refill: now }
        });

        // Refill tokens based on elapsed time
        let elapsed = now.duration_since(bucket.last_refill);
        if elapsed >= interval {
            let refills = (elapsed.as_secs() / interval.as_secs()) as u32;
            bucket.tokens = (bucket.tokens + refills).min(burst);
            bucket.last_refill = now;
        }

        if bucket.tokens > 0 {
            bucket.tokens -= 1;
            true
        } else {
            false
        }
    }
}
```

---

## Phase 4: Network Signal Handling

### 4.1 Network Handler (`collector-core/src/network_handler.rs`)

```rust
pub enum NetworkEvent {
    Connect(ConnectEvent),
    Accept(ConnectEvent),
    Close(ConnectEvent),
    Listen(ConnectEvent),
}

/// Processes network events and updates the connection tracker.
pub async fn run_network_handler(
    mut rx: mpsc::Receiver<NetworkEvent>,
    conn_tracker: Arc<Mutex<ConnTracker>>,
    cancel: CancellationToken,
) {
    loop {
        tokio::select! {
            _ = cancel.cancelled() => break,
            event = rx.recv() => {
                let event = match event {
                    Some(e) => e,
                    None => break,
                };
                if let Some((conn, is_add)) = parse_network_event(&event) {
                    let now_us = timestamp_us();
                    conn_tracker.lock().unwrap().update_connection(conn, now_us, is_add);
                }
            }
        }
    }
}

fn parse_network_event(event: &NetworkEvent) -> Option<(Connection, bool)> {
    let (connect_event, is_add) = match event {
        NetworkEvent::Connect(e) => (e, true),
        NetworkEvent::Accept(e) => (e, true),
        NetworkEvent::Close(e) => (e, false),
        NetworkEvent::Listen(e) => return None, // Handled separately for endpoints
    };

    // Check syscall succeeded
    if connect_event.retval < 0 {
        return None;
    }

    let container_id = extract_container_id_from_cgroup(&connect_event.cgroup, connect_event.cgroup_len)?;

    // Skip host events
    if container_id.is_empty() {
        return None;
    }

    let (local, remote) = parse_addresses(connect_event)?;

    // Skip loopback
    if remote.address.is_loopback() {
        return None;
    }

    let protocol = match connect_event.protocol {
        libc::IPPROTO_TCP => L4Protocol::Tcp,
        libc::IPPROTO_UDP => L4Protocol::Udp,
        _ => return None,
    };

    let role = infer_role(event, &local, &remote);

    Some((Connection {
        container_id: ContainerId::new(container_id),
        local,
        remote,
        protocol,
        role,
    }, is_add))
}
```

### 4.2 Connection Tracker (`collector-core/src/conn_tracker.rs`)

```rust
use std::collections::HashMap;
use std::time::Duration;
use collector_types::connection::*;
use collector_types::address::*;

/// Connection tracker with delta computation and afterglow support.
///
/// Design improvement over C++: uses Rust's type system to distinguish
/// raw vs normalized connections, preventing accidental mixing.
pub struct ConnTracker {
    connections: HashMap<Connection, ConnStatus>,
    endpoints: HashMap<ContainerEndpoint, ConnStatus>,
    ignored_ports: Vec<(L4Protocol, u16)>,
    ignored_networks: Vec<IpNetwork>,
    non_aggregated_networks: Vec<IpNetwork>,
    known_public_ips: Vec<IpAddr>,
    known_networks: Vec<IpNetwork>,
    afterglow_period: Duration,
}

/// Represents a normalized connection (type-safe wrapper preventing
/// mixing with raw connections).
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct NormalizedConnection(Connection);

impl ConnTracker {
    pub fn new(afterglow_period: Duration) -> Self { /* ... */ }

    /// Update a single connection from a BPF event.
    pub fn update_connection(&mut self, conn: Connection, timestamp_us: u64, active: bool) {
        let status = ConnStatus::new(timestamp_us, active);
        self.connections
            .entry(conn)
            .and_modify(|s| *s = status)
            .or_insert(status);
    }

    /// Snapshot the current state, optionally normalizing and clearing inactive.
    pub fn fetch_state(&mut self, normalize: bool, clear_inactive: bool)
        -> HashMap<Connection, ConnStatus>
    {
        let mut result = HashMap::new();

        self.connections.retain(|conn, status| {
            if !self.should_include(conn) {
                return !clear_inactive;
            }

            let key = if normalize {
                self.normalize(conn)
            } else {
                conn.clone()
            };

            result.entry(key)
                .and_modify(|existing: &mut ConnStatus| {
                    // Keep the most recent / most active status
                    if status.timestamp_us() > existing.timestamp_us() {
                        *existing = *status;
                    }
                })
                .or_insert(*status);

            // Retain if active or if we're not clearing
            !clear_inactive || status.is_active()
        });

        result
    }

    /// Compute delta between old and new states with afterglow support.
    pub fn compute_delta(
        &self,
        old: &HashMap<Connection, ConnStatus>,
        new: &HashMap<Connection, ConnStatus>,
    ) -> Vec<ConnectionUpdate> {
        let mut updates = Vec::new();
        let now_us = timestamp_us();

        // Find additions (in new but not in old, or was inactive and now active)
        for (conn, new_status) in new {
            match old.get(conn) {
                None => {
                    if new_status.is_active() {
                        updates.push(ConnectionUpdate::Added(conn.clone()));
                    }
                }
                Some(old_status) => {
                    if new_status.is_active() && !old_status.is_active()
                        && !old_status.in_afterglow(now_us, self.afterglow_period) {
                        updates.push(ConnectionUpdate::Added(conn.clone()));
                    }
                }
            }
        }

        // Find removals (in old but not in new, or was active and now inactive)
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
                        && !new_status.in_afterglow(now_us, self.afterglow_period) {
                        updates.push(ConnectionUpdate::Removed(conn.clone()));
                    }
                }
            }
        }

        updates
    }

    fn normalize(&self, conn: &Connection) -> Connection {
        let mut normalized = conn.clone();
        match conn.role {
            Role::Server => {
                // Aggregate clients: clear remote to CIDR, clear remote port
                normalized.remote = Endpoint {
                    address: self.aggregate_address(conn.remote.address),
                    port: 0,
                };
            }
            Role::Client => {
                // Aggregate by destination: clear local endpoint
                normalized.local = Endpoint {
                    address: IpAddr::V4(Ipv4Addr::UNSPECIFIED),
                    port: 0,
                };
            }
            Role::Unknown => {
                // Use ephemeral port heuristic for UDP
                if is_ephemeral_port(conn.local.port) && !is_ephemeral_port(conn.remote.port) {
                    normalized.local = Endpoint {
                        address: IpAddr::V4(Ipv4Addr::UNSPECIFIED),
                        port: 0,
                    };
                }
            }
        }
        normalized
    }

    fn should_include(&self, conn: &Connection) -> bool {
        // Check ignored ports
        for (proto, port) in &self.ignored_ports {
            if conn.protocol == *proto
                && (conn.local.port == *port || conn.remote.port == *port) {
                return false;
            }
        }
        // Check ignored networks
        for net in &self.ignored_networks {
            if net.contains(conn.remote.address) || net.contains(conn.local.address) {
                return false;
            }
        }
        true
    }
}

pub enum ConnectionUpdate {
    Added(Connection),
    Removed(Connection),
}
```

**Design improvements:**
- `NormalizedConnection` newtype prevents accidentally comparing raw and normalized connections
- `ConnStatus::in_afterglow()` is a method instead of external computation
- `fetch_state` combines snapshotting and cleanup in one pass instead of separate operations
- No mutex in the type itself - the caller (network handler) manages synchronization

---

## Phase 5: gRPC Communication

### 5.1 Signal Client (`collector-core/src/signal_client.rs`)

```rust
use tonic::transport::Channel;
use tonic::Streaming;
use tokio::sync::mpsc;

/// Trait for sending process signals. Mockable for tests.
#[async_trait::async_trait]
pub trait SignalSender: Send + Sync {
    async fn send_process_signal(&self, signal: ProcessSignal) -> Result<(), SendError>;
    async fn send_existing_processes(&self, processes: Vec<ProcessSignal>) -> Result<(), SendError>;
}

/// gRPC signal stream client with automatic reconnection.
pub struct GrpcSignalClient {
    channel: Channel,
    tx: mpsc::Sender<SignalStreamMessage>,
}

impl GrpcSignalClient {
    pub fn new(channel: Channel) -> Self { /* ... */ }

    /// Spawn the background stream management task.
    /// Returns a sender that the process handler uses.
    pub fn spawn(self, cancel: CancellationToken) -> mpsc::Sender<SignalStreamMessage> {
        let (tx, rx) = mpsc::channel(1024);

        tokio::spawn(async move {
            self.run_stream_loop(rx, cancel).await;
        });

        tx
    }

    async fn run_stream_loop(
        &self,
        mut rx: mpsc::Receiver<SignalStreamMessage>,
        cancel: CancellationToken,
    ) {
        loop {
            if cancel.is_cancelled() { break; }

            // Establish stream with retry
            let stream = match self.connect_with_retry(&cancel).await {
                Some(s) => s,
                None => break, // cancelled
            };

            // Send existing processes on fresh connection
            tracing::info!("Signal stream connected, sending existing processes");
            // ... trigger refresh via callback ...

            // Pump messages until stream breaks
            loop {
                tokio::select! {
                    _ = cancel.cancelled() => return,
                    msg = rx.recv() => {
                        match msg {
                            None => return,
                            Some(m) => {
                                if let Err(e) = stream.send(m).await {
                                    tracing::warn!("Signal stream write failed: {e}");
                                    break; // Reconnect
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
```

**Design improvements:**
- Uses `tokio::sync::mpsc` channel instead of `StoppableThread` + condition variable + atomic flag
- `CancellationToken` replaces the global atomic `g_control` + pipe-based signaling
- Reconnection is a simple loop instead of a separate thread with mutex-guarded state
- `SignalSender` trait makes testing trivial - just assert on what was sent

### 5.2 Network Client (`collector-core/src/network_client.rs`)

```rust
/// Periodically fetches connection state, computes delta, and sends to Sensor.
/// Also receives control messages (public IPs, ignored networks) from Sensor.
pub async fn run_network_client(
    channel: Channel,
    conn_tracker: Arc<Mutex<ConnTracker>>,
    config: Arc<RwLock<Config>>,
    cancel: CancellationToken,
) {
    let mut previous_state = HashMap::new();
    let scrape_interval = config.read().await.scrape_interval;

    loop {
        // Connect to sensor
        let (mut sink, mut source) = match establish_network_stream(&channel, &cancel).await {
            Some(s) => s,
            None => break,
        };

        // Spawn a task to handle inbound control messages
        let tracker_for_control = conn_tracker.clone();
        let control_task = tokio::spawn(async move {
            while let Some(msg) = source.message().await.ok().flatten() {
                apply_control_message(&tracker_for_control, msg);
            }
        });

        // Periodic delta reporting loop
        let mut interval = tokio::time::interval(scrape_interval);
        loop {
            tokio::select! {
                _ = cancel.cancelled() => {
                    control_task.abort();
                    return;
                }
                _ = interval.tick() => {
                    let current_state = {
                        conn_tracker.lock().unwrap()
                            .fetch_state(true, true)
                    };

                    let delta = ConnTracker::compute_delta_static(
                        &previous_state, &current_state
                    );

                    if !delta.is_empty() {
                        let msg = build_network_info_message(&delta);
                        if let Err(e) = sink.send(msg).await {
                            tracing::warn!("Network stream write failed: {e}");
                            break; // Reconnect
                        }
                    }

                    previous_state = current_state;
                }
            }
        }

        control_task.abort();
    }
}
```

---

## Phase 6: Configuration and Management

### 6.1 Configuration (`collector-core/src/config.rs`)

Replace `CollectorArgs` + `CollectorConfig` + `ConfigLoader` with a unified approach using `clap` + `serde`.

```rust
use clap::Parser;
use serde::Deserialize;
use std::path::PathBuf;
use std::time::Duration;

/// Command-line arguments.
#[derive(Parser, Debug)]
#[command(name = "collector")]
pub struct CliArgs {
    /// Sensor gRPC address (host:port)
    #[arg(long, env = "GRPC_SERVER")]
    pub grpc_server: Option<String>,

    /// Path to runtime config file (YAML)
    #[arg(long, default_value = "/etc/stackrox/runtime_config.yaml",
          env = "ROX_COLLECTOR_CONFIG_PATH")]
    pub config_file: PathBuf,

    /// TLS certificates directory
    #[arg(long, env = "ROX_COLLECTOR_TLS_CERTS")]
    pub tls_certs: Option<PathBuf>,

    /// TLS CA certificate path
    #[arg(long, env = "ROX_COLLECTOR_TLS_CA")]
    pub tls_ca: Option<PathBuf>,

    /// TLS client certificate path
    #[arg(long, env = "ROX_COLLECTOR_TLS_CLIENT_CERT")]
    pub tls_client_cert: Option<PathBuf>,

    /// TLS client key path
    #[arg(long, env = "ROX_COLLECTOR_TLS_CLIENT_KEY")]
    pub tls_client_key: Option<PathBuf>,

    /// Log level
    #[arg(long, default_value = "info", env = "ROX_COLLECTOR_LOG_LEVEL")]
    pub log_level: String,

    /// Host root mount path
    #[arg(long, default_value = "/host", env = "COLLECTOR_HOST_ROOT")]
    pub host_root: PathBuf,

    /// Collector config as JSON string
    #[arg(long, env = "COLLECTOR_CONFIG")]
    pub collector_config: Option<String>,

    /// BPF ring buffer total size in bytes
    /// (C++ equivalent: ROX_COLLECTOR_SINSP_TOTAL_BUFFER_SIZE)
    #[arg(long, default_value = "536870912",
          env = "ROX_COLLECTOR_SINSP_TOTAL_BUFFER_SIZE")]
    pub bpf_buffer_size: usize,

    /// CPUs per ring buffer
    #[arg(long, env = "ROX_COLLECTOR_SINSP_CPU_PER_BUFFER")]
    pub cpus_per_buffer: Option<usize>,

    /// Max process table entries
    #[arg(long, default_value = "32768",
          env = "ROX_COLLECTOR_SINSP_THREAD_CACHE_SIZE")]
    pub process_table_size: usize,
}

All environment variable names match the C++ code exactly:

| Env Var | C++ Source | Purpose |
|---------|-----------|---------|
| `GRPC_SERVER` | `CollectorConfig.cpp:74` | Sensor address |
| `COLLECTOR_HOST_ROOT` | `Utility.cpp:148` | Host filesystem mount |
| `COLLECTOR_CONFIG` | `CollectorConfig.cpp:75` | JSON config blob |
| `ROX_COLLECTOR_CONFIG_PATH` | `ConfigLoader.cpp:15` | Runtime config YAML path |
| `ROX_COLLECTOR_TLS_CERTS` | `CollectorConfig.cpp:79` | TLS certs directory |
| `ROX_COLLECTOR_TLS_CA` | `CollectorConfig.cpp:80` | TLS CA cert |
| `ROX_COLLECTOR_TLS_CLIENT_CERT` | `CollectorConfig.cpp:81` | TLS client cert |
| `ROX_COLLECTOR_TLS_CLIENT_KEY` | `CollectorConfig.cpp:82` | TLS client key |
| `ROX_COLLECTOR_LOG_LEVEL` | `CollectorConfig.cpp:71` | Log level |
| `ROX_COLLECTOR_SINSP_TOTAL_BUFFER_SIZE` | `CollectorConfig.cpp:376` | Ring buffer size |
| `ROX_COLLECTOR_SINSP_CPU_PER_BUFFER` | `CollectorConfig.cpp:358` | CPUs per buffer |
| `ROX_COLLECTOR_SINSP_THREAD_CACHE_SIZE` | `CollectorConfig.cpp:385` | Process table size |

/// Runtime configuration (loaded from YAML, hot-reloadable).
#[derive(Debug, Clone, Deserialize)]
#[serde(default)]
pub struct RuntimeConfig {
    /// ROX_COLLECTOR_SCRAPE_INTERVAL (int, seconds)
    pub scrape_interval: Duration,
    /// ROX_AFTERGLOW_PERIOD (float, seconds)
    pub afterglow_period: Duration,
    /// ROX_ENABLE_AFTERGLOW (bool)
    pub enable_afterglow: bool,
    /// ROX_COLLECTOR_EXTERNAL_IPS_ENABLE (bool)
    pub enable_external_ips: bool,
    /// Set via CollectorConfig.Networking.max_connections_per_minute
    pub max_connections_per_minute: u32,
    /// ROX_COLLECTOR_NO_PROCESS_ARGUMENTS (bool)
    pub disable_process_args: bool,
    /// ROX_COLLECT_CONNECTION_STATUS (bool)
    pub collect_connection_status: bool,
    /// ROX_COLLECTOR_TRACK_SEND_RECV (bool)
    pub track_send_recv: bool,
    /// ROX_COLLECTOR_DISABLE_NETWORK_FLOWS (bool)
    pub disable_network_flows: bool,
    /// ROX_NETWORK_GRAPH_PORTS (bool)
    pub enable_ports: bool,
    /// ROX_NETWORK_DROP_IGNORED (bool)
    pub network_drop_ignored: bool,
    /// ROX_IGNORE_NETWORKS (comma-separated CIDRs)
    pub ignored_networks: Vec<String>,
    /// ROX_NON_AGGREGATED_NETWORKS (comma-separated CIDRs)
    pub non_aggregated_networks: Vec<String>,
    /// ROX_COLLECTOR_SCRAPE_DISABLED (bool)
    pub scrape_disabled: bool,
    /// ROX_PROCESSES_LISTENING_ON_PORT (bool)
    pub processes_listening_on_port: bool,
    /// ROX_COLLECTOR_SET_IMPORT_USERS (bool)
    pub import_users: bool,
    /// ROX_COLLECTOR_ENABLE_CONNECTION_STATS (bool)
    pub enable_connection_stats: bool,
    /// ROX_COLLECTOR_ENABLE_DETAILED_METRICS (bool)
    pub enable_detailed_metrics: bool,
    /// ROX_COLLECTOR_INTROSPECTION_ENABLE (bool)
    pub enable_introspection: bool,
}

impl Default for RuntimeConfig {
    fn default() -> Self {
        Self {
            scrape_interval: Duration::from_secs(30),
            afterglow_period: Duration::from_secs(300),
            enable_external_ips: false,
            max_connections_per_minute: 2048,
            disable_process_args: false,
            collect_connection_status: false,
            track_send_recv: false,
        }
    }
}
```

**Design improvement**: The C++ version has configuration spread across 4 classes (`CollectorArgs`, `CollectorConfig`, `ConfigLoader`, environment variables). The Rust version uses `clap` with `env` attributes to unify CLI args and environment variables in one struct, and `serde` for the runtime config file. Hot-reload uses `tokio::fs::watch` + `Arc<RwLock<RuntimeConfig>>`.

### 6.2 Config File Watcher

```rust
/// Watch the config file for changes and apply them.
pub async fn watch_config_file(
    path: PathBuf,
    config: Arc<RwLock<RuntimeConfig>>,
    cancel: CancellationToken,
) {
    use tokio::io::AsyncReadExt;
    use inotify::{Inotify, WatchMask};

    let mut inotify = Inotify::init().expect("inotify init");
    inotify.watches().add(&path, WatchMask::MODIFY | WatchMask::CREATE)
        .expect("inotify watch");

    let mut buffer = vec![0u8; 4096];

    loop {
        tokio::select! {
            _ = cancel.cancelled() => break,
            result = inotify.read_events(&mut buffer) => {
                if let Ok(_events) = result {
                    match load_config_file(&path).await {
                        Ok(new_config) => {
                            *config.write().await = new_config;
                            tracing::info!("Runtime config reloaded");
                        }
                        Err(e) => {
                            tracing::warn!("Failed to reload config: {e}");
                        }
                    }
                }
            }
        }
    }
}
```

### 6.3 Health Server (`collector-core/src/health.rs`)

Replace CivetServer + Prometheus exposer with `axum`:

```rust
use axum::{routing::get, Router};
use prometheus::{Encoder, TextEncoder, Registry};

pub fn build_health_router(registry: Registry) -> Router {
    Router::new()
        .route("/healthz", get(|| async { "ok" }))
        .route("/metrics", get(move || async move {
            let encoder = TextEncoder::new();
            let mut buffer = Vec::new();
            let metric_families = registry.gather();
            encoder.encode(&metric_families, &mut buffer).unwrap();
            String::from_utf8(buffer).unwrap()
        }))
        .route("/loglevel", get(get_log_level).post(set_log_level))
}
```

---

## Phase 7: Integration and Packaging

### 7.1 Main Entry Point (`collector-bin/src/main.rs`)

```rust
use clap::Parser;
use tokio_util::sync::CancellationToken;
use collector_core::*;

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let args = config::CliArgs::parse();

    // Initialize logging
    tracing_subscriber::fmt()
        .with_env_filter(&args.log_level)
        .init();

    tracing::info!(
        version = env!("CARGO_PKG_VERSION"),
        "Collector starting"
    );

    let cancel = CancellationToken::new();

    // Register signal handlers
    let cancel_for_signal = cancel.clone();
    tokio::spawn(async move {
        tokio::signal::ctrl_c().await.ok();
        tracing::info!("Shutdown signal received");
        cancel_for_signal.cancel();
    });

    // Load runtime config
    let runtime_config = Arc::new(RwLock::new(
        config::load_config_file(&args.config_file).await.unwrap_or_default()
    ));

    // Start config file watcher
    tokio::spawn(config::watch_config_file(
        args.config_file.clone(), runtime_config.clone(), cancel.clone()
    ));

    // Initialize BPF
    let bpf_source = collector_bpf::BpfLoader::new(&args)?;

    // Self-check: verify BPF is working
    self_check::verify_bpf(&bpf_source, cancel.clone()).await?;

    // Create channels
    let (process_tx, process_rx) = mpsc::channel(4096);
    let (network_tx, network_rx) = mpsc::channel(4096);

    // Create shared connection tracker
    let conn_tracker = Arc::new(Mutex::new(
        ConnTracker::new(runtime_config.read().await.afterglow_period)
    ));

    // Start event reader (blocking thread)
    let reader_cancel = cancel.clone();
    let reader_handle = event_reader::spawn_event_reader(
        Box::new(bpf_source), process_tx, network_tx, reader_cancel,
    );

    // Connect to Sensor
    let channel = if let Some(ref addr) = args.grpc_server {
        Some(grpc::connect(addr, &args).await?)
    } else {
        None
    };

    // Start handlers
    let process_handle = tokio::spawn(process_handler::run_process_handler(
        process_rx,
        build_signal_sender(channel.clone()),
        cancel.clone(),
    ));

    let network_handle = tokio::spawn(network_handler::run_network_handler(
        network_rx, conn_tracker.clone(), cancel.clone(),
    ));

    // Start network client (periodic delta reporting)
    let network_client_handle = if let Some(ch) = channel.clone() {
        Some(tokio::spawn(network_client::run_network_client(
            ch, conn_tracker.clone(), runtime_config.clone(), cancel.clone(),
        )))
    } else {
        None
    };

    // Start health server
    let health_router = health::build_health_router(metrics::registry());
    let health_handle = tokio::spawn(
        axum::serve(
            tokio::net::TcpListener::bind("0.0.0.0:8080").await?,
            health_router,
        ).with_graceful_shutdown(cancel.cancelled_owned())
    );

    // Wait for shutdown
    cancel.cancelled().await;
    tracing::info!("Shutting down...");

    // Join all tasks
    reader_handle.join().ok();
    process_handle.await.ok();
    network_handle.await.ok();
    if let Some(h) = network_client_handle { h.await.ok(); }
    health_handle.await.ok();

    tracing::info!("Collector stopped");
    Ok(())
}
```

### 7.2 Prometheus Metrics

The Rust collector must expose the same Prometheus metrics as the C++ version to preserve dashboard and alerting compatibility. The C++ `CollectorStats` singleton tracks two categories of metrics:

**Timer metrics** (gauge family: `rox_collector_timers`):
- `net_scrape_read`, `net_scrape_update`, `net_fetch_state`, `net_create_message`, `net_write_message`, `process_info_wait`
- Each exposed as three gauges: `_events` (count), `_times_us_total`, `_times_us_avg`

**Counter metrics** (gauge family: `rox_collector_counters`):
- `net_conn_updates`, `net_conn_deltas`, `net_conn_inactive`, `net_conn_rate_limited`
- `net_cep_updates`, `net_cep_deltas`, `net_cep_inactive`
- `net_known_ip_networks`, `net_known_public_ips`
- `process_lineage_counts`, `process_lineage_total`, `process_lineage_sqr_total`, `process_lineage_string_total`
- `process_info_hit`, `process_info_miss`
- `rate_limit_flushing_counts`
- `procfs_could_not_open_fd_dir`, `procfs_could_not_open_proc_dir`, `procfs_could_not_open_pid_dir`
- `procfs_could_not_get_network_namespace`, `procfs_could_not_get_socket_inodes`
- `procfs_could_not_read_exe`, `procfs_could_not_read_cmdline`, `procfs_zombie_process`
- `event_timestamp_distant_past`, `event_timestamp_future`

**Per-event-type metrics** (when `ROX_COLLECTOR_ENABLE_DETAILED_METRICS` is true):
- `rox_collector_events` gauge family with `{type}` label per event type
- `rox_collector_event_times_total_us` and `rox_collector_event_times_avg_us` for timing

**Process lineage metrics** (gauge family: `rox_collector_process_lineage_info`):
- `lineage_count`, `lineage_avg`, `std_dev`, `lineage_avg_string_len`

**Connection rate metrics** (summary family: `rox_connections_total`):
- Dimensions: `{direction, peer_type}` where direction is `inbound`/`outbound` and peer_type is `private`/`public`
- Configurable quantiles via `ROX_COLLECTOR_CONNECTION_STATS_QUANTILES`, error via `ROX_COLLECTOR_CONNECTION_STATS_ERROR`, window via `ROX_COLLECTOR_CONNECTION_STATS_WINDOW`

In Rust, we use the `prometheus` crate to register these metric families with identical names and labels:

```rust
// crates/collector-core/src/metrics.rs

use prometheus::{Registry, GaugeVec, Opts, IntGaugeVec};
use std::sync::LazyLock;

pub static REGISTRY: LazyLock<Registry> = LazyLock::new(Registry::new);

pub static COLLECTOR_COUNTERS: LazyLock<GaugeVec> = LazyLock::new(|| {
    let opts = Opts::new("rox_collector_counters", "Collector internal counters");
    let gauge = GaugeVec::new(opts, &["type"]).unwrap();
    REGISTRY.register(Box::new(gauge.clone())).unwrap();
    gauge
});

pub static COLLECTOR_TIMERS: LazyLock<GaugeVec> = LazyLock::new(|| {
    let opts = Opts::new("rox_collector_timers", "Collector internal timers");
    let gauge = GaugeVec::new(opts, &["type", "metric"]).unwrap();
    REGISTRY.register(Box::new(gauge.clone())).unwrap();
    gauge
});

pub static COLLECTOR_EVENTS: LazyLock<GaugeVec> = LazyLock::new(|| {
    let opts = Opts::new("rox_collector_events", "Per-event-type counters");
    let gauge = GaugeVec::new(opts, &["type"]).unwrap();
    REGISTRY.register(Box::new(gauge.clone())).unwrap();
    gauge
});

pub fn registry() -> &'static Registry {
    &REGISTRY
}
```

The health server (section 6.3) exposes these at `/metrics` in Prometheus text format, identical to the C++ version's `prometheus::Exposer`.

---

## BPF Program Design

### 11.1 Overview

We write our own BPF programs instead of using the falcosecurity-libs driver. This gives us:
- **Structured events**: BPF sends well-typed structs directly, no parameter table decoding
- **Minimal overhead**: We only attach to the syscalls we care about
- **Container filtering in-kernel**: Skip host events before they reach userspace
- **CO-RE portability**: Use `vmlinux.h` + BTF for cross-kernel compatibility

### 11.2 Multi-Architecture Support (x86_64, aarch64, s390x, ppc64le)

The collector must support four architectures: **x86_64**, **aarch64** (ARM), **s390x**, and **ppc64le**. Both the Rust userspace code and the BPF programs must work on all four.

#### Rust Userspace

Rust natively supports cross-compilation to all four targets:
- `x86_64-unknown-linux-gnu`
- `aarch64-unknown-linux-gnu`
- `s390x-unknown-linux-gnu`
- `powerpc64le-unknown-linux-gnu`

No architecture-specific code is needed in the userspace Rust - all types use standard Rust primitives and `std::net` types. The CI pipeline builds separate container images per architecture (same as the existing C++ pipeline).

#### BPF Programs

BPF bytecode is architecture-independent at the instruction level (eBPF has its own ISA), but there are critical architecture-specific concerns:

1. **`vmlinux.h` generation**: Must be generated per-architecture from each target kernel's BTF. CO-RE relocations handle struct layout differences between kernels, but the initial `vmlinux.h` must come from a kernel that has the structs we reference. In practice, we generate `vmlinux.h` from a recent kernel on each arch during the build container setup, and CO-RE handles the rest at load time.

2. **`PT_REGS_PARM*` macros**: kprobe argument access differs per architecture. `bpf_tracing.h` provides architecture-aware `PT_REGS_PARM1()` etc. that handle this automatically when compiling with `-target bpf`. We must use these macros consistently (never raw register access).

3. **Endianness**: s390x is **big-endian** while the others are little-endian. This affects:
   - Network byte order conversions (`__bpf_ntohs`, `__bpf_ntohl`) - these are correct on all architectures since they convert from network (big-endian) to host order.
   - The `#[repr(C)]` shared structs between BPF and Rust: multi-byte fields are in host byte order on both sides (since both run on the same machine), so this is naturally correct.
   - IP address bytes in `sockaddr_in`/`sockaddr_in6` are in network byte order on all architectures, so `bpf_probe_read` of address bytes works the same everywhere.

4. **Syscall hook names**: The kprobe function names can differ across architectures:
   - `__sys_connect` vs `__se_sys_connect` vs `__do_sys_connect` depending on kernel version and arch
   - Mitigation: use `SEC("ksyscall/connect")` (libbpf auto-resolves the correct function) or use tracepoints (`tp_btf/sys_enter_connect`) which are arch-independent.

5. **Stack size limits**: BPF programs have a 512-byte stack limit on all architectures. Our use of per-CPU array maps for scratch space (`exec_heap`, `connect_heap`) avoids this - no arch-specific concern.

#### Build Strategy

```
# Build BPF programs once with clang -target bpf (architecture-independent bytecode)
clang -target bpf -D__TARGET_ARCH_${ARCH} -g -O2 -c collector.bpf.c -o collector.bpf.o

# Build Rust for each target
cargo build --target x86_64-unknown-linux-gnu
cargo build --target aarch64-unknown-linux-gnu
cargo build --target s390x-unknown-linux-gnu
cargo build --target powerpc64le-unknown-linux-gnu
```

The `libbpf-cargo` build script handles BPF compilation. We pass `-D__TARGET_ARCH_x86` etc. as appropriate, which `bpf_tracing.h` uses to select the right `PT_REGS` definitions.

#### CI Validation

Each architecture must have dedicated CI testing:
- BPF program loading tests on real hardware or QEMU emulation
- `s390x` big-endian correctness tests for network address parsing
- Verify all kprobe/tracepoint hook names resolve on each arch's kernel

### 11.3 BPF Program (`crates/collector-bpf/src/bpf/collector.bpf.c`)

```c
// collector.bpf.c - All BPF programs for the collector
//
// Uses CO-RE (Compile Once Run Everywhere) with vmlinux.h for portability.
// Events are sent to userspace via a BPF ring buffer.

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

// Must match Rust-side definitions in events.rs
#define MAX_FILENAME_LEN 256
#define MAX_ARGS_LEN 1024
#define MAX_CGROUP_LEN 256
#define TASK_COMM_LEN 16

#define EVENT_EXEC    1
#define EVENT_EXIT    2
#define EVENT_FORK    3
#define EVENT_CONNECT 10
#define EVENT_ACCEPT  11
#define EVENT_CLOSE   12
#define EVENT_LISTEN  13

// ============================================================
// Shared structures (must match #[repr(C)] Rust structs)
// ============================================================

struct event_header {
    __u32 event_type;
    __u64 timestamp_ns;
    __u32 pid;       // tgid (userspace PID)
    __u32 tid;       // tid (kernel thread ID)
    __u32 uid;
    __u32 gid;
};

struct exec_event {
    struct event_header header;
    __u32 ppid;
    __u32 filename_len;
    char filename[MAX_FILENAME_LEN];
    __u32 args_len;
    char args[MAX_ARGS_LEN];
    char comm[TASK_COMM_LEN];
    __u32 cgroup_len;
    char cgroup[MAX_CGROUP_LEN];
};

struct connect_event {
    struct event_header header;
    __u16 socket_family;
    __u16 protocol;
    __u8  src_addr[16];
    __u8  dst_addr[16];
    __u16 src_port;
    __u16 dst_port;
    __s32 retval;
    __u32 cgroup_len;
    char  cgroup[MAX_CGROUP_LEN];
};

struct exit_event {
    struct event_header header;
    __s32 exit_code;
};

// ============================================================
// Maps
// ============================================================

// Ring buffer for sending events to userspace
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 16 * 1024 * 1024); // 16MB, tunable from userspace
} events SEC(".maps");

// Per-CPU scratch space for building events (too large for stack)
struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, struct exec_event);
} exec_heap SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, struct connect_event);
} connect_heap SEC(".maps");

// ============================================================
// Helpers
// ============================================================

static __always_inline void fill_header(struct event_header *hdr, __u32 type) {
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    __u64 uid_gid = bpf_get_current_uid_gid();

    hdr->event_type = type;
    hdr->timestamp_ns = bpf_ktime_get_boot_ns();
    hdr->pid = pid_tgid >> 32;       // tgid = userspace PID
    hdr->tid = (__u32)pid_tgid;      // tid
    hdr->uid = (__u32)uid_gid;
    hdr->gid = uid_gid >> 32;
}

// Read the cgroup name from the current task.
// We read from the first cgroup subsystem (cpu or memory).
static __always_inline __u32 read_cgroup(char *buf, __u32 buf_len) {
    struct task_struct *task = (void *)bpf_get_current_task();

    // Navigate: task->cgroups->subsys[0]->cgroup->kn->name
    struct css_set *cgroups;
    BPF_CORE_READ_INTO(&cgroups, task, cgroups);
    if (!cgroups) return 0;

    struct cgroup_subsys_state *subsys;
    BPF_CORE_READ_INTO(&subsys, cgroups, subsys[0]);
    if (!subsys) return 0;

    struct cgroup *cgrp;
    BPF_CORE_READ_INTO(&cgrp, subsys, cgroup);
    if (!cgrp) return 0;

    struct kernfs_node *kn;
    BPF_CORE_READ_INTO(&kn, cgrp, kn);
    if (!kn) return 0;

    // Read the full path by walking up kernfs_node parents.
    // For simplicity, just read the leaf name which contains the container ID.
    const char *name;
    BPF_CORE_READ_INTO(&name, kn, name);
    if (!name) return 0;

    int ret = bpf_probe_read_kernel_str(buf, buf_len, name);
    return ret > 0 ? ret : 0;
}

// ============================================================
// Process exec tracepoint
// ============================================================

// We attach to sched_process_exec which fires after a successful execve.
// This replaces the falcosecurity PPME_SYSCALL_EXECVE_19_X event.
SEC("tp_btf/sched_process_exec")
int handle_exec(struct trace_event_raw_sched_process_exec *ctx) {
    __u32 zero = 0;
    struct exec_event *evt = bpf_map_lookup_elem(&exec_heap, &zero);
    if (!evt) return 0;

    __builtin_memset(evt, 0, sizeof(*evt));
    fill_header(&evt->header, EVENT_EXEC);

    struct task_struct *task = (void *)bpf_get_current_task();

    // Parent PID
    struct task_struct *parent;
    BPF_CORE_READ_INTO(&parent, task, real_parent);
    BPF_CORE_READ_INTO(&evt->ppid, parent, tgid);

    // comm
    bpf_get_current_comm(evt->comm, sizeof(evt->comm));

    // filename from linux_binprm
    // ctx->__data contains the filename offset. Use the BPF helper approach:
    unsigned int filename_loc = ctx->__data_loc_filename;
    __u16 offset = filename_loc & 0xFFFF;
    __u16 len = (filename_loc >> 16) & 0xFFFF;
    if (len > MAX_FILENAME_LEN - 1) len = MAX_FILENAME_LEN - 1;
    bpf_probe_read_kernel_str(evt->filename, len + 1,
                               (void *)ctx + offset);
    evt->filename_len = len;

    // Process arguments: read from /proc/self/cmdline equivalent
    // We read from the mm->arg_start to mm->arg_end
    struct mm_struct *mm;
    BPF_CORE_READ_INTO(&mm, task, mm);
    if (mm) {
        unsigned long arg_start, arg_end;
        BPF_CORE_READ_INTO(&arg_start, mm, arg_start);
        BPF_CORE_READ_INTO(&arg_end, mm, arg_end);
        unsigned long arg_len = arg_end - arg_start;
        if (arg_len > MAX_ARGS_LEN) arg_len = MAX_ARGS_LEN;
        bpf_probe_read_user(evt->args, arg_len, (void *)arg_start);
        evt->args_len = arg_len;
    }

    // Cgroup for container ID resolution
    evt->cgroup_len = read_cgroup(evt->cgroup, sizeof(evt->cgroup));

    // Submit to ring buffer
    bpf_ringbuf_output(&events, evt, sizeof(*evt), 0);
    return 0;
}

// ============================================================
// Process exit tracepoint
// ============================================================

SEC("tp_btf/sched_process_exit")
int handle_exit(struct trace_event_raw_sched_process_template *ctx) {
    struct exit_event evt = {};
    fill_header(&evt.header, EVENT_EXIT);

    struct task_struct *task = (void *)bpf_get_current_task();
    BPF_CORE_READ_INTO(&evt.exit_code, task, exit_code);

    // Only report thread group leaders (main thread exit = process exit)
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    if ((__u32)pid_tgid != (pid_tgid >> 32))
        return 0; // Skip non-leader thread exits

    bpf_ringbuf_output(&events, &evt, sizeof(evt), 0);
    return 0;
}

// ============================================================
// Network: connect (kprobe/kretprobe approach)
//
// We use a two-phase approach:
//   1. kprobe on sys_connect captures the sockaddr argument
//   2. kretprobe on sys_connect captures the return value
//   3. Combine and emit event
// ============================================================

// Temporary storage for connect arguments between entry and exit
struct connect_args {
    struct sockaddr *addr;
    int fd;
};

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, __u64);   // pid_tgid
    __type(value, struct connect_args);
} connect_args_map SEC(".maps");

SEC("kprobe/__sys_connect")
int kprobe_connect(struct pt_regs *ctx) {
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    struct connect_args args = {};
    args.fd = PT_REGS_PARM1(ctx);
    args.addr = (struct sockaddr *)PT_REGS_PARM2(ctx);
    bpf_map_update_elem(&connect_args_map, &pid_tgid, &args, BPF_ANY);
    return 0;
}

SEC("kretprobe/__sys_connect")
int kretprobe_connect(struct pt_regs *ctx) {
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    struct connect_args *args = bpf_map_lookup_elem(&connect_args_map, &pid_tgid);
    if (!args) return 0;

    int retval = PT_REGS_RC(ctx);
    // Allow EINPROGRESS for async connects
    if (retval < 0 && retval != -EINPROGRESS) {
        bpf_map_delete_elem(&connect_args_map, &pid_tgid);
        return 0;
    }

    __u32 zero = 0;
    struct connect_event *evt = bpf_map_lookup_elem(&connect_heap, &zero);
    if (!evt) {
        bpf_map_delete_elem(&connect_args_map, &pid_tgid);
        return 0;
    }

    __builtin_memset(evt, 0, sizeof(*evt));
    fill_header(&evt->header, EVENT_CONNECT);
    evt->retval = retval;

    // Read socket family
    __u16 family;
    bpf_probe_read_user(&family, sizeof(family), &args->addr->sa_family);
    evt->socket_family = family;

    if (family == 2 /* AF_INET */) {
        struct sockaddr_in addr4 = {};
        bpf_probe_read_user(&addr4, sizeof(addr4), args->addr);
        __builtin_memcpy(evt->dst_addr, &addr4.sin_addr, 4);
        evt->dst_port = __bpf_ntohs(addr4.sin_port);

        // Get source from socket (filled by kernel after connect)
        // We'll get this from the socket struct via fd
        // For now, source is filled by userspace from /proc/net if needed
    } else if (family == 10 /* AF_INET6 */) {
        struct sockaddr_in6 addr6 = {};
        bpf_probe_read_user(&addr6, sizeof(addr6), args->addr);
        __builtin_memcpy(evt->dst_addr, &addr6.sin6_addr, 16);
        evt->dst_port = __bpf_ntohs(addr6.sin6_port);
    } else {
        bpf_map_delete_elem(&connect_args_map, &pid_tgid);
        return 0; // Skip non-IP sockets
    }

    // Read socket protocol from the socket struct
    struct task_struct *task = (void *)bpf_get_current_task();
    struct file **fds;
    struct files_struct *files;
    BPF_CORE_READ_INTO(&files, task, files);
    // ... read fd -> socket -> sk -> sk_protocol

    evt->cgroup_len = read_cgroup(evt->cgroup, sizeof(evt->cgroup));

    bpf_ringbuf_output(&events, evt, sizeof(*evt), 0);
    bpf_map_delete_elem(&connect_args_map, &pid_tgid);
    return 0;
}

// ============================================================
// Network: accept (using tp_btf/inet_sock_set_state for cleaner approach)
// ============================================================

// For accept, we hook the point where a new socket transitions
// to TCP_ESTABLISHED state.
SEC("tp_btf/inet_sock_set_state")
int handle_inet_sock_set_state(struct trace_event_raw_inet_sock_set_state *ctx) {
    // Only care about transitions to ESTABLISHED
    if (ctx->newstate != 1 /* TCP_ESTABLISHED */)
        return 0;

    // Only care about transitions from SYN_RECV (server-side accept)
    // or SYN_SENT (client-side connect completion)
    int oldstate = ctx->oldstate;
    __u32 event_type;

    if (oldstate == 3 /* TCP_SYN_RECV */) {
        event_type = EVENT_ACCEPT;  // Server accepted a connection
    } else if (oldstate == 2 /* TCP_SYN_SENT */) {
        // Client connect completed - we already handle this via kretprobe
        return 0;
    } else {
        return 0;
    }

    __u32 zero = 0;
    struct connect_event *evt = bpf_map_lookup_elem(&connect_heap, &zero);
    if (!evt) return 0;

    __builtin_memset(evt, 0, sizeof(*evt));
    fill_header(&evt->header, event_type);
    evt->retval = 0; // successful accept
    evt->socket_family = ctx->family;
    evt->protocol = IPPROTO_TCP;

    if (ctx->family == 2) {
        __builtin_memcpy(evt->src_addr, ctx->saddr, 4);
        __builtin_memcpy(evt->dst_addr, ctx->daddr, 4);
    } else {
        __builtin_memcpy(evt->src_addr, ctx->saddr_v6, 16);
        __builtin_memcpy(evt->dst_addr, ctx->daddr_v6, 16);
    }
    evt->src_port = ctx->sport;
    evt->dst_port = __bpf_ntohs(ctx->dport);

    evt->cgroup_len = read_cgroup(evt->cgroup, sizeof(evt->cgroup));

    bpf_ringbuf_output(&events, evt, sizeof(*evt), 0);
    return 0;
}

// ============================================================
// Network: close (TCP and UDP)
// ============================================================

// The C++ collector hooks the generic close() and shutdown() syscalls,
// which cover both TCP and UDP sockets. We need two hooks:
//
// 1. kprobe/tcp_close - for TCP connections being torn down.
//    This fires specifically for TCP sockets and gives us direct
//    access to the sock struct with all connection details.
//
// 2. A generic close() hook for UDP sockets. UDP sockets don't go
//    through tcp_close, so we must intercept the close() syscall
//    and check if the fd is a UDP socket.
//
// We use a two-phase approach for the generic close: save the fd
// on entry, look up socket info and emit event on exit.

// --- TCP close: direct kernel function hook ---

SEC("kprobe/tcp_close")
int handle_tcp_close(struct pt_regs *ctx) {
    struct sock *sk = (struct sock *)PT_REGS_PARM1(ctx);

    __u32 zero = 0;
    struct connect_event *evt = bpf_map_lookup_elem(&connect_heap, &zero);
    if (!evt) return 0;

    __builtin_memset(evt, 0, sizeof(*evt));
    fill_header(&evt->header, EVENT_CLOSE);
    evt->retval = 0;

    __u16 family;
    BPF_CORE_READ_INTO(&family, sk, __sk_common.skc_family);
    evt->socket_family = family;
    evt->protocol = IPPROTO_TCP;

    BPF_CORE_READ_INTO(&evt->src_port, sk, __sk_common.skc_num);
    __u16 dport;
    BPF_CORE_READ_INTO(&dport, sk, __sk_common.skc_dport);
    evt->dst_port = __bpf_ntohs(dport);

    if (family == 2) {
        BPF_CORE_READ_INTO(evt->src_addr, sk, __sk_common.skc_rcv_saddr);
        BPF_CORE_READ_INTO(evt->dst_addr, sk, __sk_common.skc_daddr);
    } else if (family == 10) {
        BPF_CORE_READ_INTO(evt->src_addr, sk, __sk_common.skc_v6_rcv_saddr);
        BPF_CORE_READ_INTO(evt->dst_addr, sk, __sk_common.skc_v6_daddr);
    }

    evt->cgroup_len = read_cgroup(evt->cgroup, sizeof(evt->cgroup));

    bpf_ringbuf_output(&events, evt, sizeof(*evt), 0);
    return 0;
}

// --- Generic close: captures UDP and other socket closes ---

struct close_args {
    int fd;
};

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, __u64);   // pid_tgid
    __type(value, struct close_args);
} close_args_map SEC(".maps");

// On close() entry, save the fd so we can look up the socket on exit.
SEC("kprobe/__sys_close")
int kprobe_close(struct pt_regs *ctx) {
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    struct close_args args = { .fd = PT_REGS_PARM1(ctx) };
    bpf_map_update_elem(&close_args_map, &pid_tgid, &args, BPF_ANY);
    return 0;
}

// On close() exit, check if the fd was a UDP socket and emit an event.
// TCP sockets are already handled by kprobe/tcp_close above, so we
// only emit for UDP here to avoid duplicate events.
SEC("kretprobe/__sys_close")
int kretprobe_close(struct pt_regs *ctx) {
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    struct close_args *args = bpf_map_lookup_elem(&close_args_map, &pid_tgid);
    if (!args) return 0;

    int retval = PT_REGS_RC(ctx);
    if (retval < 0) {
        bpf_map_delete_elem(&close_args_map, &pid_tgid);
        return 0;
    }

    // Look up the fd in the task's file descriptor table to find the socket.
    // We need to read: task->files->fdt->fd[fd] -> file->f_inode
    // then check if it's a socket and extract the sock struct.
    struct task_struct *task = (void *)bpf_get_current_task();
    struct files_struct *files;
    BPF_CORE_READ_INTO(&files, task, files);
    if (!files) goto cleanup;

    struct fdtable *fdt;
    BPF_CORE_READ_INTO(&fdt, files, fdt);
    if (!fdt) goto cleanup;

    struct file **fd_array;
    BPF_CORE_READ_INTO(&fd_array, fdt, fd);
    if (!fd_array) goto cleanup;

    struct file *filp;
    bpf_probe_read_kernel(&filp, sizeof(filp), &fd_array[args->fd]);
    if (!filp) goto cleanup;

    // Check if this is a socket by reading the file operations or inode type.
    // Navigate: file -> private_data (for sockets, this is the struct socket)
    // -> socket -> sk -> sk_protocol
    struct socket *sock;
    BPF_CORE_READ_INTO(&sock, filp, private_data);
    if (!sock) goto cleanup;

    struct sock *sk;
    BPF_CORE_READ_INTO(&sk, sock, sk);
    if (!sk) goto cleanup;

    // Only handle UDP - TCP is already covered by kprobe/tcp_close
    __u8 sk_protocol;
    BPF_CORE_READ_INTO(&sk_protocol, sk, sk_protocol);
    if (sk_protocol != IPPROTO_UDP) goto cleanup;

    __u16 family;
    BPF_CORE_READ_INTO(&family, sk, __sk_common.skc_family);
    if (family != 2 && family != 10) goto cleanup; // Only IPv4/IPv6

    __u32 zero = 0;
    struct connect_event *evt = bpf_map_lookup_elem(&connect_heap, &zero);
    if (!evt) goto cleanup;

    __builtin_memset(evt, 0, sizeof(*evt));
    fill_header(&evt->header, EVENT_CLOSE);
    evt->retval = 0;
    evt->socket_family = family;
    evt->protocol = IPPROTO_UDP;

    BPF_CORE_READ_INTO(&evt->src_port, sk, __sk_common.skc_num);
    __u16 udp_dport;
    BPF_CORE_READ_INTO(&udp_dport, sk, __sk_common.skc_dport);
    evt->dst_port = __bpf_ntohs(udp_dport);

    if (family == 2) {
        BPF_CORE_READ_INTO(evt->src_addr, sk, __sk_common.skc_rcv_saddr);
        BPF_CORE_READ_INTO(evt->dst_addr, sk, __sk_common.skc_daddr);
    } else {
        BPF_CORE_READ_INTO(evt->src_addr, sk, __sk_common.skc_v6_rcv_saddr);
        BPF_CORE_READ_INTO(evt->dst_addr, sk, __sk_common.skc_v6_daddr);
    }

    evt->cgroup_len = read_cgroup(evt->cgroup, sizeof(evt->cgroup));
    bpf_ringbuf_output(&events, evt, sizeof(*evt), 0);

cleanup:
    bpf_map_delete_elem(&close_args_map, &pid_tgid);
    return 0;
}

// --- Shutdown: covers both TCP and UDP ---
// shutdown() is less common but the C++ collector also hooks it.
// We use the same fd-lookup approach as close().

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 4096);
    __type(key, __u64);
    __type(value, struct close_args);
} shutdown_args_map SEC(".maps");

SEC("kprobe/__sys_shutdown")
int kprobe_shutdown(struct pt_regs *ctx) {
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    struct close_args args = { .fd = PT_REGS_PARM1(ctx) };
    bpf_map_update_elem(&shutdown_args_map, &pid_tgid, &args, BPF_ANY);
    return 0;
}

// kretprobe for shutdown follows the same pattern as close.
// Implementation mirrors kretprobe_close but reads from shutdown_args_map
// and emits for both TCP and UDP (since tcp_close won't fire for shutdown).
```

### 11.3 BPF Build Integration (`crates/collector-bpf/build.rs`)

```rust
use libbpf_cargo::SkeletonBuilder;

fn main() {
    SkeletonBuilder::new()
        .source("src/bpf/collector.bpf.c")
        .clang_args([
            "-I", "src/bpf/",        // vmlinux.h location
            "-target", "bpf",
            "-g",                      // BTF debug info
            "-O2",
        ])
        .build_and_generate("src/bpf/collector.bpf.skel.rs")
        .expect("BPF skeleton build failed");
}
```

### 11.4 Why These Hook Points

| Event | Hook | Rationale |
|-------|------|-----------|
| Process exec | `tp_btf/sched_process_exec` | Fires after successful execve, has access to the new binary's filename and the task struct. More reliable than kretprobe on execve. |
| Process exit | `tp_btf/sched_process_exit` | Clean notification of process termination. Filter to thread group leaders only. |
| Connect | `kprobe/__sys_connect` + `kretprobe` | Two-phase: capture sockaddr on entry, retval on exit. Standard pattern for syscall tracing. Use `ksyscall/connect` for arch-portable hook names. |
| Accept | `tp_btf/inet_sock_set_state` | Cleaner than hooking accept() syscall. Fires when socket transitions to ESTABLISHED from SYN_RECV (server-side). |
| TCP Close | `kprobe/tcp_close` | Direct hook on TCP close. Efficient because it only fires for TCP sockets. |
| UDP Close | `kprobe/__sys_close` + `kretprobe` | Generic close() hook with fd lookup to find socket. Filters to UDP-only (TCP handled above). Needed because UDP sockets don't go through `tcp_close`. |
| Shutdown | `kprobe/__sys_shutdown` + `kretprobe` | Handles `shutdown()` syscall for both TCP and UDP. Same fd-lookup pattern as generic close. |

### 11.5 Comparison with Falcosecurity-libs Driver

| Aspect | Falcosecurity-libs | Our BPF Programs |
|--------|-------------------|-----------------|
| Event format | Opaque byte buffer with parameter table | Typed `#[repr(C)]` structs |
| Parsing | Requires libscap + libsinsp | Zero-copy deserialize in Rust |
| Container ID | Plugin + filtercheck extraction | Direct cgroup read in BPF |
| Thread table | Full thread/fd table in libsinsp | Minimal ProcessTable in userspace |
| Hook points | Raw syscall enter/exit tracepoints | Targeted: exec/accept/connect/close tracepoints + kprobes |
| Ring buffer | Per-CPU buffers via libscap | Single BPF ring buffer (simpler, still efficient) |
| Filtering | Compile filter expression in userspace | Simple in-BPF checks (skip non-IP sockets) |

---

## Testing Plan

### 12.1 Unit Tests (Per-Crate)

#### `collector-types` Tests

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn conn_status_packing() {
        let status = ConnStatus::new(1_000_000, true);
        assert!(status.is_active());
        assert_eq!(status.timestamp_us(), 1_000_000);

        let status = ConnStatus::new(1_000_000, false);
        assert!(!status.is_active());
        assert_eq!(status.timestamp_us(), 1_000_000);
    }

    #[test]
    fn ip_network_contains() {
        let net = IpNetwork {
            address: "10.0.0.0".parse().unwrap(),
            prefix_len: 8,
        };
        assert!(net.contains("10.1.2.3".parse().unwrap()));
        assert!(!net.contains("11.0.0.1".parse().unwrap()));
    }

    #[test]
    fn endpoint_hash_equality() {
        // Verify that identical endpoints hash the same way
        let e1 = Endpoint { address: "1.2.3.4".parse().unwrap(), port: 80 };
        let e2 = Endpoint { address: "1.2.3.4".parse().unwrap(), port: 80 };
        assert_eq!(e1, e2);
        // ... hash equality check
    }
}
```

#### `container_id` Tests

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn docker_cgroup_v1() {
        let cgroup = "/docker/abc123def456abc123def456abc123def456abc123def456abc123def456abcd";
        assert_eq!(extract_container_id(cgroup), Some("abc123def456"));
    }

    #[test]
    fn crio_cgroup() {
        let cgroup = "crio-abc123def456abc123def456abc123def456abc123def456abc123def456abcd.scope";
        assert_eq!(extract_container_id(cgroup), Some("abc123def456"));
    }

    #[test]
    fn containerd_systemd_cgroup() {
        let id = "a" .repeat(64);
        let cgroup = format!("/system.slice/cri-containerd-{id}.scope");
        assert_eq!(extract_container_id(&cgroup), Some(&id[..12]));
    }

    #[test]
    fn host_process_no_container() {
        assert_eq!(extract_container_id("/user.slice/user-1000.slice"), None);
    }

    #[test]
    fn kubernetes_pod_cgroup() {
        let id = "b".repeat(64);
        let cgroup = format!(
            "/kubepods/besteffort/pod12345/{id}"
        );
        assert_eq!(extract_container_id(&cgroup), Some(&id[..12]));
    }
}
```

#### `process_table` Tests

```rust
#[cfg(test)]
mod tests {
    use super::*;

    fn make_process(pid: u32, ppid: u32, exe: &str, container: &str) -> ProcessInfo {
        ProcessInfo {
            pid, tid: pid, ppid, uid: 1000, gid: 1000,
            comm: exe.to_string(),
            exe_path: format!("/usr/bin/{exe}"),
            args: vec![exe.to_string()],
            cwd: "/".to_string(),
            container_id: ContainerId::new(container),
            cgroup: String::new(),
        }
    }

    #[test]
    fn lineage_stops_at_container_boundary() {
        let mut table = ProcessTable::new(100);
        table.upsert(make_process(1, 0, "systemd", ""));       // host
        table.upsert(make_process(100, 1, "containerd", ""));   // host
        table.upsert(make_process(200, 100, "bash", "abc123")); // container
        table.upsert(make_process(300, 200, "curl", "abc123")); // container

        let lineage = table.lineage(300, "abc123", 10);
        assert_eq!(lineage.len(), 1);
        assert_eq!(lineage[0].parent_exec_file_path, "/usr/bin/bash");
    }

    #[test]
    fn lineage_collapses_consecutive_same_exe() {
        let mut table = ProcessTable::new(100);
        table.upsert(make_process(1, 0, "bash", "abc123"));
        table.upsert(make_process(2, 1, "bash", "abc123")); // same exe
        table.upsert(make_process(3, 2, "bash", "abc123")); // same exe
        table.upsert(make_process(4, 3, "curl", "abc123"));

        let lineage = table.lineage(4, "abc123", 10);
        assert_eq!(lineage.len(), 1); // collapsed to single bash entry
    }

    #[test]
    fn lineage_max_depth() {
        let mut table = ProcessTable::new(100);
        for i in 0..20 {
            table.upsert(make_process(i, i.saturating_sub(1),
                         &format!("proc{i}"), "abc123"));
        }
        let lineage = table.lineage(19, "abc123", 10);
        assert!(lineage.len() <= 10);
    }

    #[test]
    fn eviction_removes_oldest() {
        let mut table = ProcessTable::new(3);
        table.upsert(make_process(1, 0, "a", "c1"));
        std::thread::sleep(Duration::from_millis(1));
        table.upsert(make_process(2, 0, "b", "c1"));
        std::thread::sleep(Duration::from_millis(1));
        table.upsert(make_process(3, 0, "c", "c1"));
        // Table is full. Adding a 4th should evict PID 1.
        table.upsert(make_process(4, 0, "d", "c1"));
        assert!(table.get(1).is_none());
        assert!(table.get(4).is_some());
    }

    #[test]
    fn iter_returns_all_processes() {
        let mut table = ProcessTable::new(100);
        table.upsert(make_process(1, 0, "a", "c1"));
        table.upsert(make_process(2, 1, "b", "c1"));
        let all: Vec<_> = table.iter().collect();
        assert_eq!(all.len(), 2);
    }
}
```

#### `conn_tracker` Tests

This is the most complex component. Port the existing 80KB `ConnTrackerTest.cpp` test suite, organized by behavior:

```rust
#[cfg(test)]
mod tests {
    use super::*;

    fn tcp_conn(container: &str, local_port: u16, remote_ip: &str, remote_port: u16, role: Role) -> Connection {
        Connection {
            container_id: ContainerId::new(container),
            local: Endpoint { address: "10.0.0.1".parse().unwrap(), port: local_port },
            remote: Endpoint { address: remote_ip.parse().unwrap(), port: remote_port },
            protocol: L4Protocol::Tcp,
            role,
        }
    }

    #[test]
    fn update_connection_tracks_state() {
        let mut tracker = ConnTracker::new(Duration::from_secs(300));
        let conn = tcp_conn("c1", 12345, "10.0.0.2", 80, Role::Client);
        tracker.update_connection(conn.clone(), 1000, true);

        let state = tracker.fetch_state(false, false);
        assert_eq!(state.len(), 1);
        assert!(state[&conn].is_active());
    }

    #[test]
    fn close_marks_inactive() {
        let mut tracker = ConnTracker::new(Duration::from_secs(300));
        let conn = tcp_conn("c1", 12345, "10.0.0.2", 80, Role::Client);
        tracker.update_connection(conn.clone(), 1000, true);
        tracker.update_connection(conn.clone(), 2000, false);

        let state = tracker.fetch_state(false, false);
        assert!(!state[&conn].is_active());
    }

    #[test]
    fn delta_detects_new_connection() {
        let old = HashMap::new();
        let mut new = HashMap::new();
        let conn = tcp_conn("c1", 12345, "10.0.0.2", 80, Role::Client);
        new.insert(conn.clone(), ConnStatus::new(1000, true));

        let tracker = ConnTracker::new(Duration::from_secs(0));
        let delta = tracker.compute_delta(&old, &new);
        assert_eq!(delta.len(), 1);
        assert!(matches!(delta[0], ConnectionUpdate::Added(_)));
    }

    #[test]
    fn delta_detects_removal() {
        let conn = tcp_conn("c1", 12345, "10.0.0.2", 80, Role::Client);
        let mut old = HashMap::new();
        old.insert(conn.clone(), ConnStatus::new(1000, true));
        let new = HashMap::new();

        let tracker = ConnTracker::new(Duration::from_secs(0));
        let delta = tracker.compute_delta(&old, &new);
        assert_eq!(delta.len(), 1);
        assert!(matches!(delta[0], ConnectionUpdate::Removed(_)));
    }

    #[test]
    fn afterglow_suppresses_rapid_changes() {
        let conn = tcp_conn("c1", 12345, "10.0.0.2", 80, Role::Client);
        let now = 5_000_000; // 5 seconds in microseconds

        let mut old = HashMap::new();
        old.insert(conn.clone(), ConnStatus::new(now - 1_000_000, true)); // active 1s ago

        let mut new = HashMap::new();
        new.insert(conn.clone(), ConnStatus::new(now, false)); // just went inactive

        let tracker = ConnTracker::new(Duration::from_secs(300));
        let delta = tracker.compute_delta(&old, &new);
        // Should be empty because the connection is within the afterglow period
        assert!(delta.is_empty());
    }

    #[test]
    fn normalization_aggregates_server_clients() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        // Two different clients connecting to our server port 80
        tracker.update_connection(
            tcp_conn("c1", 80, "10.0.0.2", 45000, Role::Server), 1000, true);
        tracker.update_connection(
            tcp_conn("c1", 80, "10.0.0.3", 45001, Role::Server), 1000, true);

        let state = tracker.fetch_state(true, false);
        // After normalization, both should merge (remote cleared)
        assert_eq!(state.len(), 1);
    }

    #[test]
    fn ignored_ports_filtered() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        tracker.set_ignored_ports(vec![(L4Protocol::Tcp, 22)]);
        tracker.update_connection(
            tcp_conn("c1", 12345, "10.0.0.2", 22, Role::Client), 1000, true);

        let state = tracker.fetch_state(false, false);
        assert!(state.is_empty()); // SSH connection filtered out
    }

    #[test]
    fn ignored_networks_filtered() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        tracker.set_ignored_networks(vec![
            IpNetwork { address: "172.17.0.0".parse().unwrap(), prefix_len: 16 }
        ]);
        tracker.update_connection(
            tcp_conn("c1", 12345, "172.17.0.5", 80, Role::Client), 1000, true);

        let state = tracker.fetch_state(false, false);
        assert!(state.is_empty());
    }

    #[test]
    fn clear_inactive_removes_old_entries() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        let conn = tcp_conn("c1", 12345, "10.0.0.2", 80, Role::Client);
        tracker.update_connection(conn.clone(), 1000, true);
        tracker.update_connection(conn.clone(), 2000, false); // now inactive

        let state = tracker.fetch_state(false, true); // clear_inactive=true
        assert!(!state.is_empty()); // still in snapshot

        // But internal map should have been cleaned
        let state2 = tracker.fetch_state(false, false);
        assert!(state2.is_empty());
    }

    #[test]
    fn udp_role_inference_from_ephemeral_port() {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));
        let conn = Connection {
            container_id: ContainerId::new("c1"),
            local: Endpoint { address: "10.0.0.1".parse().unwrap(), port: 45000 },
            remote: Endpoint { address: "10.0.0.2".parse().unwrap(), port: 53 },
            protocol: L4Protocol::Udp,
            role: Role::Unknown,
        };
        tracker.update_connection(conn, 1000, true);

        let state = tracker.fetch_state(true, false);
        // After normalization with unknown role, ephemeral port heuristic should
        // treat local port 45000 as client (clear local endpoint)
        for (conn, _) in &state {
            assert_eq!(conn.local.port, 0); // cleared = client
        }
    }
}
```

#### `rate_limit` Tests

```rust
#[cfg(test)]
mod tests {
    #[test]
    fn allows_burst_then_blocks() {
        let mut limiter = RateLimitCache::new(3, Duration::from_secs(60));
        assert!(limiter.allow("key1"));
        assert!(limiter.allow("key1"));
        assert!(limiter.allow("key1"));
        assert!(!limiter.allow("key1")); // Burst exhausted
    }

    #[test]
    fn different_keys_independent() {
        let mut limiter = RateLimitCache::new(1, Duration::from_secs(60));
        assert!(limiter.allow("key1"));
        assert!(limiter.allow("key2")); // Independent bucket
        assert!(!limiter.allow("key1")); // key1 exhausted
    }

    #[test]
    fn refills_after_interval() {
        // Use a mock clock or very short interval for testing
        let mut limiter = RateLimitCache::new(1, Duration::from_millis(10));
        assert!(limiter.allow("key1"));
        assert!(!limiter.allow("key1"));
        std::thread::sleep(Duration::from_millis(15));
        assert!(limiter.allow("key1")); // Refilled
    }
}
```

### 12.2 Integration Tests

#### BPF Self-Check Test

```rust
// tests/bpf_self_check.rs
// Requires root privileges and BPF support.

#[test]
#[ignore] // Run with: cargo test -- --ignored
fn bpf_programs_load_and_capture_exec() {
    let mut loader = BpfLoader::new(&default_args()).unwrap();
    let (tx, rx) = std::sync::mpsc::channel();

    // Start event capture
    let handle = std::thread::spawn(move || {
        for _ in 0..10 {
            if let Some(event) = loader.next_event(1000) {
                tx.send(event).ok();
            }
        }
    });

    // Trigger an exec event
    std::process::Command::new("/bin/true").output().unwrap();

    handle.join().unwrap();

    // Should have captured at least one exec event
    let events: Vec<_> = rx.try_iter().collect();
    assert!(events.iter().any(|e| matches!(e, RawEvent::Exec(_))));
}

#[test]
#[ignore]
fn bpf_programs_capture_network_connect() {
    let mut loader = BpfLoader::new(&default_args()).unwrap();

    // Start capture in background
    // ... connect to a known address ...
    // ... verify connect event received ...
}
```

#### End-to-End Test with Mock Sensor

```rust
// tests/e2e_mock_sensor.rs

use tonic::transport::Server;

/// Mock Sensor that records received signals.
struct MockSignalService {
    received: Arc<Mutex<Vec<SignalStreamMessage>>>,
}

#[tonic::async_trait]
impl SignalService for MockSignalService {
    type PushSignalsStream = ReceiverStream<Result<Empty, Status>>;

    async fn push_signals(
        &self,
        request: Request<Streaming<SignalStreamMessage>>,
    ) -> Result<Response<Self::PushSignalsStream>, Status> {
        let mut stream = request.into_inner();
        let received = self.received.clone();
        let (tx, rx) = mpsc::channel(1);

        tokio::spawn(async move {
            while let Some(Ok(msg)) = stream.next().await {
                received.lock().unwrap().push(msg);
            }
        });

        Ok(Response::new(ReceiverStream::new(rx)))
    }
}

#[tokio::test]
#[ignore]
async fn exec_event_reaches_mock_sensor() {
    // 1. Start mock sensor
    let received = Arc::new(Mutex::new(Vec::new()));
    let service = MockSignalService { received: received.clone() };
    let addr = "127.0.0.1:0".parse().unwrap();
    let server = Server::builder()
        .add_service(SignalServiceServer::new(service))
        .serve(addr);
    let server_addr = /* get bound address */;

    // 2. Start collector pointing at mock sensor (using MockEventSource)
    let mock_source = MockEventSource::new(vec![
        RawEvent::Exec(make_exec_event("bash", "/usr/bin/bash", "abc123def456")),
    ]);

    // 3. Run collector for a short time
    // 4. Assert mock sensor received the process signal
    let signals = received.lock().unwrap();
    assert_eq!(signals.len(), 1);
    // Verify protobuf fields match
}
```

#### Mock Event Source for Testing

```rust
/// A mock EventSource for testing without BPF.
pub struct MockEventSource {
    events: VecDeque<RawEvent>,
}

impl MockEventSource {
    pub fn new(events: Vec<RawEvent>) -> Self {
        Self { events: events.into() }
    }
}

impl EventSource for MockEventSource {
    fn next_event(&mut self, _timeout_ms: i32) -> Option<RawEvent> {
        self.events.pop_front()
    }
}
```

### 12.3 Property-Based Tests

```rust
// Use proptest for connection tracker invariants

use proptest::prelude::*;

proptest! {
    #[test]
    fn delta_is_consistent(
        connections in prop::collection::vec(arb_connection(), 0..100),
        timestamps in prop::collection::vec(1000u64..1_000_000, 0..100),
    ) {
        let mut tracker = ConnTracker::new(Duration::from_secs(0));

        for (conn, ts) in connections.iter().zip(timestamps.iter()) {
            tracker.update_connection(conn.clone(), *ts, true);
        }

        let state1 = tracker.fetch_state(false, false);

        // Remove half the connections
        for (i, (conn, ts)) in connections.iter().zip(timestamps.iter()).enumerate() {
            if i % 2 == 0 {
                tracker.update_connection(conn.clone(), ts + 1000, false);
            }
        }

        let state2 = tracker.fetch_state(false, false);
        let delta = tracker.compute_delta(&state1, &state2);

        // Invariant: every removal in delta should correspond to a connection
        // that was active in state1 and inactive in state2
        for update in &delta {
            if let ConnectionUpdate::Removed(conn) = update {
                assert!(state1.get(conn).map_or(false, |s| s.is_active()));
                assert!(state2.get(conn).map_or(true, |s| !s.is_active()));
            }
        }
    }
}

fn arb_connection() -> impl Strategy<Value = Connection> {
    (
        "[a-f0-9]{12}",           // container_id
        1u16..65535,               // local_port
        (1u8..255, 0u8..255, 0u8..255, 1u8..255), // remote IP
        1u16..65535,               // remote_port
        prop_oneof![Just(Role::Client), Just(Role::Server)],
    ).prop_map(|(cid, lport, (a,b,c,d), rport, role)| {
        Connection {
            container_id: ContainerId::new(&cid),
            local: Endpoint {
                address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 1)),
                port: lport,
            },
            remote: Endpoint {
                address: IpAddr::V4(Ipv4Addr::new(a, b, c, d)),
                port: rport,
            },
            protocol: L4Protocol::Tcp,
            role,
        }
    })
}
```

### 12.4 Benchmark Tests

```rust
// benches/conn_tracker.rs
use criterion::{criterion_group, criterion_main, Criterion};

fn bench_connection_tracking(c: &mut Criterion) {
    c.bench_function("update_1000_connections", |b| {
        b.iter(|| {
            let mut tracker = ConnTracker::new(Duration::from_secs(300));
            for i in 0..1000u16 {
                let conn = Connection {
                    container_id: ContainerId::new("abc123"),
                    local: Endpoint { address: "10.0.0.1".parse().unwrap(), port: i },
                    remote: Endpoint { address: "10.0.0.2".parse().unwrap(), port: 80 },
                    protocol: L4Protocol::Tcp,
                    role: Role::Client,
                };
                tracker.update_connection(conn, i as u64 * 1000, true);
            }
        });
    });

    c.bench_function("fetch_and_normalize_1000", |b| {
        let mut tracker = ConnTracker::new(Duration::from_secs(300));
        for i in 0..1000u16 {
            let conn = Connection {
                container_id: ContainerId::new("abc123"),
                local: Endpoint { address: "10.0.0.1".parse().unwrap(), port: i },
                remote: Endpoint { address: "10.0.0.2".parse().unwrap(), port: 80 },
                protocol: L4Protocol::Tcp,
                role: Role::Client,
            };
            tracker.update_connection(conn, i as u64 * 1000, true);
        }
        b.iter(|| {
            tracker.fetch_state(true, false)
        });
    });

    c.bench_function("compute_delta_1000", |b| {
        // ... setup old and new states with 1000 connections ...
        b.iter(|| {
            tracker.compute_delta(&old_state, &new_state)
        });
    });
}

fn bench_container_id_extraction(c: &mut Criterion) {
    let cgroup = "/kubepods/besteffort/pod12345/abc123def456abc123def456abc123def456abc123def456abc123def456abcd";
    c.bench_function("extract_container_id", |b| {
        b.iter(|| extract_container_id(cgroup))
    });
}

fn bench_rate_limiter(c: &mut Criterion) {
    c.bench_function("rate_limit_10000_keys", |b| {
        b.iter(|| {
            let mut limiter = RateLimitCache::new(10, Duration::from_secs(1800));
            for i in 0..10_000 {
                limiter.allow(&format!("key_{i}"));
            }
        });
    });
}

criterion_group!(benches, bench_connection_tracking, bench_container_id_extraction, bench_rate_limiter);
criterion_main!(benches);
```

### 12.5 Test Categories Summary

| Category | Location | Count | Requires |
|----------|----------|-------|----------|
| Unit: types | `collector-types/src/` | ~20 | Nothing |
| Unit: container_id | `collector-core/src/container_id.rs` | ~10 | Nothing |
| Unit: process_table | `collector-core/src/process_table.rs` | ~10 | Nothing |
| Unit: conn_tracker | `collector-core/src/conn_tracker.rs` | ~40 | Nothing |
| Unit: rate_limit | `collector-core/src/rate_limit.rs` | ~5 | Nothing |
| Unit: config parsing | `collector-core/src/config.rs` | ~5 | Nothing |
| Property: conn_tracker | `collector-core/tests/` | ~5 | proptest |
| Integration: BPF | `collector-bin/tests/` | ~5 | root, BPF |
| Integration: E2E | `collector-bin/tests/` | ~5 | root, BPF |
| Benchmark | `benches/` | ~5 | criterion |

---

## 13. Migration Strategy

### Phase-by-Phase Delivery

Each phase produces a testable artifact. The Rust binary can coexist with the C++ binary during transition.

| Phase | Deliverable | Can Test Independently? |
|-------|------------|----------------------|
| 1. Foundation | Types crate + BPF programs load | Yes (unit tests + BPF load test) |
| 2. Event Pipeline | Events flow from BPF to channels | Yes (BPF integration test) |
| 3. Process Handling | Process signals sent to stdout | Yes (run binary, check stdout) |
| 4. Network Handling | Connection tracker with delta | Yes (unit tests, stdout output) |
| 5. gRPC | Full communication with Sensor | Yes (deploy alongside C++ collector) |
| 6. Config & Management | Hot-reload, metrics, health | Yes (full feature parity) |
| 7. Integration | Container image, CI/CD | Yes (integration test suite) |

### Parallel Running

During migration, both collectors can run simultaneously on different nodes in the same cluster. The Sensor doesn't care which language the collector is written in - it only sees protobuf messages. This allows gradual rollout with easy rollback.

### Feature Parity Validation

Before cutting over, verify:
1. Same process signals generated for identical workloads
2. Same network connections reported
3. Same metrics exposed
4. Same configuration options supported
5. Performance within acceptable bounds (< 2x CPU, < 2x memory of C++ version)

---

## 14. Key Design Decisions

### 14.1 Why `libbpf-rs` Over `aya`

Both are viable. Recommendation: **`libbpf-rs`** because:
- Closer to upstream libbpf (same BTF/CO-RE semantics)
- `libbpf-cargo` generates type-safe Rust skeletons from BPF C code
- More mature for production use
- The BPF programs are still written in C (where the ecosystem is strongest), with a Rust userspace wrapper

If `aya` is preferred (pure Rust BPF programs), the architecture supports swapping - the `EventSource` trait abstracts the BPF layer.

### 14.2 Why Channels Over Handler Chain

The C++ version uses a synchronous handler chain: each event is dispatched to handlers in sequence on the main thread. This means:
- A slow handler blocks all subsequent handlers
- Process and network handlers share a thread
- The sinsp mutex must be held during handler execution

Channels decouple producers from consumers:
- Event reader runs on its own thread at full speed
- Process handler and network handler run independently
- Back-pressure is natural (channel capacity)
- No mutex needed between handlers

### 14.3 Why `tokio` Over OS Threads

The C++ version uses 5-6 OS threads with hand-rolled synchronization. The Rust version uses:
- 1 OS thread for BPF ring buffer polling (blocking I/O, not suitable for async)
- `tokio` tasks for everything else (gRPC, config watching, metrics, network reporting)

This is simpler (no `StoppableThread` base class needed), more efficient (fewer OS threads), and composes better with the Rust async ecosystem (tonic, axum, tokio-inotify).

### 14.4 Why Direct Cgroup Parsing Over Container Plugin

The falcosecurity-libs container plugin is a shared library loaded at runtime that provides `container.id` and `k8s.ns.name`. It requires:
- Specific loading order (before EventExtractor init)
- A custom filter factory to support plugin fields
- Runtime dependency on `libcontainer.so`

Direct cgroup parsing in BPF:
- Zero runtime dependencies
- Works with all container runtimes
- Testable without BPF (pure string parsing function)
- No loading order issues

The `k8s.ns.name` field that the C++ container plugin provides is not needed in the Rust collector. Kubernetes metadata (namespace, deployment, pod labels) is resolved by Sensor, not by Collector. Collector only provides the `container_id`, and Sensor maps that to the Kubernetes context. This means we can drop the `k8s.ns.name` extraction entirely.

### 14.5 No ProtoAllocator Needed

The C++ version uses arena allocation for protobuf messages to reduce heap pressure. Rust's ownership model makes this unnecessary:
- Protobuf messages are stack-allocated or single-owner heap-allocated
- No GC, so no GC pressure to optimize for
- The `prost` crate generates plain Rust structs, not arena-aware objects
- If allocation becomes a bottleneck, we can use `bumpalo` or object pools, but measure first

---

## 15. Risk Register

| Risk | Impact | Mitigation |
|------|--------|------------|
| BPF programs don't capture all events that falco driver does | Missing security signals | Comprehensive BPF integration tests comparing output with C++ collector |
| `libbpf-rs` ring buffer performance insufficient | Event drops under load | Benchmark early, tune ring buffer size, consider per-CPU buffers |
| Cgroup parsing misses container runtime variants | Processes not attributed to containers | Extensive test suite with Docker, CRI-O, containerd, podman cgroup formats |
| gRPC reconnection edge cases | Lost signals during network issues | Stress test with network partition simulation |
| tokio runtime overhead for BPF polling | Latency increase | BPF reader on dedicated OS thread (already planned) |
| Missing events during collector restart | Gap in monitoring | Existing process scan on startup (same as C++ version) |
| BTF not available on older kernels | BPF programs fail to load | Require kernel 5.8+ (same as current CO-RE BPF requirement) |
| Multi-arch BPF differences (s390x big-endian, kprobe names) | BPF programs fail to load or produce wrong data on non-x86 | Use `ksyscall/` helpers for arch-portable hook names; use `__bpf_ntohs`/`__bpf_ntohl` consistently; CI testing on all 4 architectures; s390x-specific endianness tests for IP address parsing |
| Generic close() hook performance on high-fd-churn workloads | Overhead from hooking all close() calls, not just sockets | The kretprobe checks if fd is a UDP socket and exits early for non-sockets. Benchmark on high-churn workloads to validate. If problematic, consider hooking `udp_destroy_sock` instead. |

---

## Appendix: Dependency Versions

```toml
# Cargo.toml (workspace)
[workspace]
members = ["crates/*"]
resolver = "2"

[workspace.dependencies]
# BPF
libbpf-rs = "0.24"
libbpf-cargo = "0.24"

# Async
tokio = { version = "1", features = ["full"] }
tokio-util = { version = "0.7", features = ["rt"] }
tokio-stream = "0.1"

# gRPC
tonic = { version = "0.12", features = ["tls"] }
tonic-build = "0.12"
prost = "0.13"
prost-types = "0.13"

# HTTP
axum = "0.7"

# Config
clap = { version = "4", features = ["derive", "env"] }
serde = { version = "1", features = ["derive"] }
serde_yaml = "0.9"

# Observability
tracing = "0.1"
tracing-subscriber = { version = "0.3", features = ["env-filter"] }
prometheus = "0.13"

# Utilities
anyhow = "1"
uuid = { version = "1", features = ["v4"] }
async-trait = "0.1"

# Testing
proptest = "1"
criterion = "0.5"
```


## Appendix B: Protobuf Code Generation for Rust

The protobuf definitions live in `collector/proto/` and must generate Rust code. We use `tonic-build` (which wraps `prost-build`) to generate both message structs and gRPC service client stubs.

### Build Script (`collector-core/build.rs`)

```rust
fn main() -> Result<(), Box<dyn std::error::Error>> {
    tonic_build::configure()
        .build_server(false)  // We only need client stubs (collector is a client)
        .build_client(true)
        .compile_protos(
            &[
                // Signal service (process signals)
                "proto/internalapi/sensor/signal_iservice.proto",
                // Network connection service
                "proto/internalapi/sensor/network_connection_iservice.proto",
                // Supporting types
                "proto/api/v1/signal.proto",
                "proto/api/v1/empty.proto",
                "proto/internalapi/sensor/collector.proto",
                "proto/internalapi/sensor/network_connection_info.proto",
                "proto/internalapi/sensor/network_enums.proto",
                "proto/storage/network_flow.proto",
                "proto/storage/process_indicator.proto",
            ],
            &[
                "proto/",                     // Include path for imports
            ],
        )?;
    Ok(())
}
```

### Generated Code Usage

`tonic-build` generates Rust structs (via `prost`) and gRPC client stubs (via `tonic`):

```rust
// Include the generated code
pub mod api {
    pub mod v1 {
        tonic::include_proto!("v1");
    }
}

pub mod sensor {
    tonic::include_proto!("sensor");
}

pub mod storage {
    tonic::include_proto!("storage");
}

// Usage example: building a ProcessSignal
use storage::ProcessSignal;
use prost_types::Timestamp;

let signal = ProcessSignal {
    id: uuid::Uuid::new_v4().to_string(),
    container_id: "abc123def456".to_string(),
    time: Some(Timestamp {
        seconds: now.as_secs() as i64,
        nanos: now.subsec_nanos() as i32,
    }),
    name: "bash".to_string(),
    exec_file_path: "/usr/bin/bash".to_string(),
    args: "-c echo hello".to_string(),
    pid: 1234,
    uid: 1000,
    gid: 1000,
    scraped: false,
    lineage_info: vec![],
    ..Default::default()
};

// Usage example: gRPC client
use sensor::signal_service_client::SignalServiceClient;

let mut client = SignalServiceClient::connect("https://sensor:9443").await?;
let stream = tokio_stream::iter(vec![signal_stream_message]);
let response = client.push_signals(stream).await?;
```

### Cargo.toml Dependencies

```toml
[build-dependencies]
tonic-build = "0.12"

[dependencies]
prost = "0.13"
prost-types = "0.13"    # For google.protobuf.Timestamp
tonic = { version = "0.12", features = ["tls"] }
```

### Proto File Location

The proto files are symlinks into the StackRox proto definitions. For the Rust build, the `proto/` directory from the existing C++ project is reused directly - no changes to the proto definitions are needed. The `tonic-build` compiler reads the same `.proto` files that the C++ `protobuf_generate()` CMake command uses.
