## Goal: Convert Collector from C++ to Rust

### Type
epic

### Priority
0

### Description
Replace the C++ collector (built on falcosecurity-libs) with a Rust implementation using custom BPF programs via libbpf-rs. Must maintain wire compatibility with Sensor (identical protobuf messages and gRPC services). Must support x86_64, aarch64, s390x, and ppc64le architectures.

See plan.md for the full design document.

### Acceptance Criteria
- Rust collector produces identical protobuf messages to Sensor as the C++ version
- All process signals (execve) captured and sent via gRPC
- All network connections (TCP/UDP connect, accept, close, shutdown) tracked with delta reporting
- Rate limiting, afterglow, connection normalization all working
- Prometheus metrics with same names as C++ version
- Multi-arch support (x86_64, aarch64, s390x, ppc64le)
- Integration tests pass against mock Sensor

## Set up Cargo workspace and crate structure

### Type
task

### Priority
1

### Description
Create the Cargo workspace with four crates as described in plan.md section 3:
- `collector-bpf` - BPF programs and skeleton loader
- `collector-types` - Shared types (no heavy dependencies)
- `collector-core` - Main application logic
- `collector-bin` - Binary entry point

Set up workspace-level dependencies in root Cargo.toml. Include all dependencies listed in plan.md Appendix A (libbpf-rs, tokio, tonic, prost, axum, clap, serde, tracing, prometheus, etc).

### Labels
phase-1

## Set up protobuf code generation with tonic-build

### Type
task

### Priority
1

### Description
Configure tonic-build in collector-core/build.rs to compile the existing .proto files from collector/proto/ into Rust structs and gRPC client stubs. Must compile all proto files:
- api/v1/signal.proto, empty.proto
- internalapi/sensor/signal_iservice.proto, network_connection_iservice.proto, collector.proto, network_connection_info.proto, network_enums.proto
- storage/network_flow.proto, process_indicator.proto

Create mod.rs with tonic::include_proto! for all packages. Verify generated code compiles.

See plan.md Appendix B for build.rs code and usage examples.

### Labels
phase-1

## Implement shared types in collector-types crate

### Type
task

### Priority
2

### Description
Implement the foundation types in collector-types (plan.md section 1.1):

- `address.rs`: Endpoint (IpAddr + port), IpNetwork (CIDR with contains()), private_networks()
- `connection.rs`: L4Protocol enum, Role enum, Connection struct, ConnStatus (packed u64 with active bit + timestamp), ContainerId newtype
- `process.rs`: ProcessInfo struct, LineageInfo struct
- `container.rs`: ContainerId type

All types must derive Debug, Clone, PartialEq, Eq, Hash as appropriate. ConnStatus must use the packed u64 representation (upper bit = active flag, lower 63 = timestamp_us).

### Labels
phase-1

### Acceptance Criteria
- All types compile with no dependencies beyond std
- Unit tests for ConnStatus packing/unpacking
- Unit tests for IpNetwork::contains()
- Unit tests for Endpoint hash equality

## Define BPF event structs shared between C and Rust

### Type
task

### Priority
2

### Description
Create the #[repr(C)] event structs in collector-bpf/src/events.rs (plan.md section 1.2):

- EventType enum (ProcessExec=1, ProcessExit=2, ProcessFork=3, SocketConnect=10, SocketAccept=11, SocketClose=12, SocketListen=13)
- EventHeader (event_type, timestamp_ns, pid, tid, uid, gid)
- ExecEvent (header, ppid, filename, args as null-separated bytes, comm, cgroup)
- ConnectEvent (header, socket_family, protocol, src/dst addr as 16-byte arrays, ports, retval, cgroup)
- ExitEvent (header, exit_code)
- RawEvent enum for parsed events

Constants: MAX_FILENAME_LEN=256, MAX_ARGS_LEN=1024, MAX_CGROUP_LEN=256.

These structs must exactly match the C struct layouts in the BPF programs.

### Labels
phase-1

## Write BPF programs for process exec and exit

### Type
task

### Priority
1

### Description
Write the BPF C programs for process monitoring in collector-bpf/src/bpf/collector.bpf.c (plan.md section 11.3):

- tp_btf/sched_process_exec handler: captures filename, args (from mm->arg_start), comm, ppid, cgroup, uid/gid. Uses per-CPU array map for scratch space (too large for stack).
- tp_btf/sched_process_exit handler: emits ExitEvent for thread group leaders only.
- Ring buffer map for event delivery.
- fill_header() helper for common fields.
- read_cgroup() helper to extract cgroup name from task->cgroups->subsys[0]->cgroup->kn->name.

Must use CO-RE (vmlinux.h + BPF_CORE_READ_INTO) for portability. Must compile for all target architectures.

### Labels
phase-1, bpf

## Write BPF programs for network events (connect, accept, close, shutdown)

### Type
task

### Priority
1

### Description
Write the BPF C programs for network monitoring in collector.bpf.c (plan.md sections 11.3):

- kprobe/__sys_connect + kretprobe: two-phase connect capture. Save sockaddr on entry, emit ConnectEvent with retval on exit. Handle AF_INET and AF_INET6. Allow EINPROGRESS for async connects. Use ksyscall/ for arch portability.
- tp_btf/inet_sock_set_state: captures TCP accepts (SYN_RECV -> ESTABLISHED transition).
- kprobe/tcp_close: TCP close events with socket tuple extraction from sock struct.
- kprobe/__sys_close + kretprobe: generic close for UDP sockets. Looks up fd -> socket -> sk, filters to UDP only (TCP handled by tcp_close).
- kprobe/__sys_shutdown + kretprobe: shutdown() for both TCP and UDP.

All network hooks must read cgroup for container ID resolution. Must handle both IPv4 and IPv6.

### Labels
phase-1, bpf

## BPF skeleton loader and EventSource trait

### Type
task

### Priority
2

### Description
Implement the BPF skeleton loader in collector-bpf/src/lib.rs (plan.md section 1.4):

- Set up libbpf-cargo build.rs to compile collector.bpf.c into a skeleton.
- BpfLoader struct that loads and attaches the BPF programs.
- EventSource trait (trait EventSource: Send { fn next_event(&mut self, timeout_ms: i32) -> Option<RawEvent>; })
- Implement EventSource for BpfLoader: polls ring buffer, deserializes #[repr(C)] structs into RawEvent enum.
- MockEventSource for testing (returns events from a VecDeque).

### Labels
phase-1

### Acceptance Criteria
- BPF programs compile via libbpf-cargo
- MockEventSource works in unit tests
- BpfLoader loads on a real kernel (integration test, requires root)

## Multi-architecture BPF build and validation

### Type
task

### Priority
2

### Description
Ensure BPF programs and Rust userspace build and work on all four architectures (plan.md section 11.2):

- Configure build.rs to pass -D__TARGET_ARCH_${ARCH} to clang
- Verify PT_REGS_PARM* macros work via bpf_tracing.h on all arches
- Validate endianness handling for s390x (big-endian): __bpf_ntohs/__bpf_ntohl usage, IP address byte order
- Set up cross-compilation targets: x86_64-unknown-linux-gnu, aarch64-unknown-linux-gnu, s390x-unknown-linux-gnu, powerpc64le-unknown-linux-gnu
- Document vmlinux.h generation per-architecture

### Labels
phase-1, multi-arch

## Implement container ID extraction from cgroup

### Type
task

### Priority
2

### Description
Implement container_id.rs in collector-core (plan.md section 2.1):

- extract_container_id(cgroup: &str) -> Option<&str>: extracts 12-char container ID from cgroup path
- Must handle: Docker (/docker/<64-hex>), CRI-O (crio-<64-hex>.scope), containerd (cri-containerd-<64-hex>.scope), systemd (docker-<64-hex>.scope), bare 64-hex path segments, kubepods paths
- Return first 12 chars of the 64-char hex ID

### Labels
phase-2

### Acceptance Criteria
- Unit tests for Docker, CRI-O, containerd, systemd, kubepods cgroup formats
- Test that host processes (no container) return None
- Test edge cases (short strings, non-hex chars)

## Implement ProcessTable

### Type
task

### Priority
2

### Description
Implement process_table.rs in collector-core (plan.md section 2.2):

- ProcessTable struct with HashMap<u32, ProcessEntry> and max_size
- upsert(info) -> Option<ProcessInfo>: insert/update, evict oldest if full
- remove(pid) -> Option<ProcessInfo>: on exit events
- get(pid) -> Option<&ProcessInfo>: lookup
- lineage(pid, container_id, max_depth) -> Vec<LineageInfo>: walk parent chain, stop at container boundary (different container_id), collapse consecutive same exe_path, max 10 ancestors
- iter() -> impl Iterator<Item = &ProcessInfo>: for SendExistingProcesses

Single-owner design - no mutex needed. LRU eviction by last_seen Instant.

### Labels
phase-2

### Acceptance Criteria
- Unit test: lineage stops at container boundary
- Unit test: lineage collapses consecutive same exe
- Unit test: lineage max depth
- Unit test: eviction removes oldest
- Unit test: iter returns all processes

## Implement event reader (BPF to channels)

### Type
task

### Priority
2

### Description
Implement event_reader.rs in collector-core (plan.md section 2.3):

- spawn_event_reader(source, process_tx, network_tx, cancel) -> JoinHandle
- Runs on a dedicated OS thread (BPF polling is blocking, not async)
- Polls EventSource::next_event(100ms timeout) in a loop
- Routes RawEvent::Exec/Exit to process_tx channel, Connect/Accept/Close/Listen to network_tx channel
- Exits when CancellationToken is cancelled

Define ProcessEvent and NetworkEvent enums for the channel types.

### Labels
phase-2

## Implement process signal handler

### Type
task

### Priority
2

### Description
Implement process_handler.rs in collector-core (plan.md section 3.1):

- run_process_handler(rx, sender, cancel) async function
- Receives ProcessEvent from channel, handles Exec and Exit
- On Exec: parse event, extract container_id (skip empty = host), skip container runtimes (runc, conmon), build lineage from ProcessTable, check rate limiter, build ProcessSignal protobuf, send via SignalSender trait
- On Exit: remove from ProcessTable
- Rate limit key: "{container_id}:{comm}:{args_first_256}:{exe_path}"
- UTF-8 sanitization on all string fields
- Track metrics: sent, rate_limited, resolution_failures, send_failures

SignalSender trait must be defined for testability (plan.md section 5.1).

### Labels
phase-3

### Acceptance Criteria
- Unit test with MockEventSource and mock SignalSender
- Test that host processes (no container_id) are filtered
- Test that runc processes are filtered
- Test rate limiting (10 bursts per 30 min window)

## Implement rate limiter

### Type
task

### Priority
2

### Description
Implement rate_limit.rs in collector-core (plan.md section 3.2):

- RateLimitCache struct with HashMap<String, TokenBucket>
- allow(key: &str) -> bool: token bucket algorithm
- Configurable burst count and refill interval
- Default: 10 bursts per 30-minute window
- max_entries limit (65536) to bound memory

### Labels
phase-3

### Acceptance Criteria
- Test: allows burst then blocks
- Test: different keys are independent
- Test: refills after interval

## Implement network signal handler

### Type
task

### Priority
2

### Description
Implement network_handler.rs in collector-core (plan.md section 4.1):

- run_network_handler(rx, conn_tracker, cancel) async function
- Receives NetworkEvent from channel
- parse_network_event(): extract Connection from ConnectEvent, determine is_add (connect/accept=true, close=false)
- Check retval >= 0 (skip failed syscalls)
- Extract container_id from cgroup (skip empty = host)
- Parse IPv4/IPv6 addresses from event struct
- Skip loopback addresses
- Determine L4Protocol from event protocol field
- Infer role (client/server) from event type
- Call conn_tracker.update_connection()

### Labels
phase-4

## Implement connection tracker

### Type
task

### Priority
1

### Description
Implement conn_tracker.rs in collector-core (plan.md section 4.2). This is the most complex component.

- ConnTracker struct with HashMap<Connection, ConnStatus> and configuration
- update_connection(conn, timestamp_us, active): insert/update ConnStatus
- fetch_state(normalize, clear_inactive) -> HashMap: snapshot with optional normalization and cleanup
- compute_delta(old, new) -> Vec<ConnectionUpdate>: diff between states with afterglow support
- normalize(): server connections aggregate by port (clear remote), client connections aggregate by destination (clear local), UDP uses ephemeral port heuristic
- Filtering: ignored_ports, ignored_networks, non_aggregated_networks
- NormalizedConnection newtype to prevent mixing raw/normalized
- Configuration update methods: set_ignored_ports, set_ignored_networks, set_afterglow_period, etc.

### Labels
phase-4

### Acceptance Criteria
- Port key test behaviors from C++ ConnTrackerTest.cpp (80KB test suite)
- Test: update tracks state, close marks inactive
- Test: delta detects additions and removals
- Test: afterglow suppresses rapid changes
- Test: normalization aggregates server clients
- Test: ignored ports/networks filtered
- Test: clear_inactive removes old entries
- Test: UDP role inference from ephemeral port
- Property-based tests for delta consistency (proptest)

## Implement gRPC signal client (process signals)

### Type
task

### Priority
2

### Description
Implement signal_client.rs in collector-core (plan.md section 5.1):

- SignalSender trait: send_process_signal(), send_existing_processes()
- GrpcSignalClient: manages bidirectional stream to Sensor's SignalService.PushSignals
- spawn() -> mpsc::Sender: spawns background tokio task for stream management
- Automatic reconnection loop with retry on write failure
- On fresh connection: trigger SendExistingProcesses (replay process table)
- StdoutSignalSender for debug mode (logs protobuf to stdout)
- CancellationToken for graceful shutdown

Uses tonic for gRPC, tokio::sync::mpsc for message passing.

### Labels
phase-5

## Implement gRPC network client (connection deltas)

### Type
task

### Priority
2

### Description
Implement network_client.rs in collector-core (plan.md section 5.2):

- run_network_client(channel, conn_tracker, config, cancel) async function
- Bidirectional stream to Sensor's NetworkConnectionInfoService.PushNetworkConnectionInfo
- Periodic delta reporting: fetch_state() from ConnTracker, compute_delta(), send NetworkConnectionInfoMessage
- Handle inbound NetworkFlowsControlMessage: apply public IP lists, known networks, ignored ports, afterglow config to ConnTracker
- Automatic reconnection on stream failure
- Scrape interval from RuntimeConfig (default 30s)

### Labels
phase-5

## Implement configuration system (clap + serde)

### Type
task

### Priority
2

### Description
Implement config.rs in collector-core (plan.md section 6.1):

- CliArgs struct using clap::Parser with all env vars matching C++ exactly (see env var table in plan.md)
- RuntimeConfig struct using serde::Deserialize with all runtime config fields and their corresponding env vars
- All env var names must be identical to C++ code (GRPC_SERVER, ROX_COLLECTOR_TLS_CA, ROX_COLLECTOR_SINSP_TOTAL_BUFFER_SIZE, etc.)
- Defaults matching C++ (scrape_interval=30s, afterglow_period=300s, process_table_size=32768, etc.)

### Labels
phase-6

## Implement config file watcher (inotify)

### Type
task

### Priority
3

### Description
Implement config file watching in collector-core (plan.md section 6.2):

- watch_config_file(path, config: Arc<RwLock<RuntimeConfig>>, cancel) async function
- Watch config file via inotify for MODIFY/CREATE events
- On change: parse YAML, update Arc<RwLock<RuntimeConfig>>
- Log warnings on parse failures (don't crash)
- Graceful shutdown via CancellationToken

### Labels
phase-6

## Implement health server and Prometheus metrics

### Type
task

### Priority
2

### Description
Implement health.rs and metrics.rs in collector-core (plan.md sections 6.3 and 7.2):

health.rs:
- axum router with /healthz, /metrics, /loglevel endpoints
- /metrics serves Prometheus text format from registry

metrics.rs - must expose same metric names as C++ version:
- rox_collector_timers gauge family (net_scrape_read, net_scrape_update, net_fetch_state, net_create_message, net_write_message, process_info_wait) with events/times_us_total/times_us_avg
- rox_collector_counters gauge family (24 counters: net_conn_updates, net_conn_deltas, process_lineage_counts, procfs errors, etc.)
- rox_collector_events gauge family (per-event-type, when detailed metrics enabled)
- rox_collector_process_lineage_info gauge family (lineage_count, lineage_avg, std_dev, lineage_avg_string_len)
- rox_connections_total summary family (direction x peer_type dimensions)

### Labels
phase-6

### Acceptance Criteria
- /healthz returns 200
- /metrics returns valid Prometheus text format
- All metric names match C++ CollectorStatsExporter.cpp

## Implement main entry point

### Type
task

### Priority
2

### Description
Implement main.rs in collector-bin (plan.md section 7.1):

- Parse CLI args with clap
- Initialize tracing logging
- Register signal handlers (SIGTERM, SIGINT via tokio::signal)
- Load runtime config from YAML
- Start config file watcher
- Initialize BPF (BpfLoader)
- Run self-check to verify BPF is working
- Create mpsc channels for process and network events
- Create shared ConnectionTracker (Arc<Mutex<>>)
- Spawn event reader on OS thread
- Connect to Sensor via gRPC (if configured)
- Spawn process handler, network handler, network client tasks
- Start health server on port 8080
- Wait for cancellation, join all tasks

### Labels
phase-7

## Implement self-check (BPF driver validation)

### Type
task

### Priority
3

### Description
Implement self_check.rs in collector-core (plan.md section 7.1):

- verify_bpf(source, cancel) -> Result<()>
- Execute a known binary (e.g. /bin/true) and verify that an exec event is received from BPF within 5 seconds
- Optionally verify network events by connecting to a known address
- Log success/failure
- Return error if no events received (BPF driver not working)

### Labels
phase-7

## Write unit tests for all components

### Type
task

### Priority
2

### Description
Write comprehensive unit tests as described in plan.md section 12:

- collector-types: ConnStatus packing, IpNetwork::contains, Endpoint hash (~20 tests)
- container_id: Docker, CRI-O, containerd, systemd, kubepods, host process (~10 tests)
- process_table: lineage boundary/collapse/depth, eviction, iter (~10 tests)
- conn_tracker: state tracking, delta, afterglow, normalization, filtering (~40 tests, port from C++ ConnTrackerTest.cpp)
- rate_limit: burst/block, key independence, refill (~5 tests)
- config: parsing, defaults (~5 tests)
- Property-based tests for conn_tracker with proptest (~5 tests)

### Labels
testing

## Write integration tests (BPF + mock Sensor)

### Type
task

### Priority
2

### Description
Write integration tests as described in plan.md section 12.2:

- BPF self-check test: load BPF programs, run /bin/true, verify exec event captured (requires root)
- BPF network test: load BPF, make TCP connection, verify connect event
- End-to-end with MockSignalService: start mock gRPC Sensor, inject events via MockEventSource, verify correct protobuf messages received
- MockEventSource helper for testing without BPF

All BPF tests marked #[ignore] (require root).

### Labels
testing

## Write benchmark tests

### Type
task

### Priority
3

### Description
Write benchmark tests using criterion as described in plan.md section 12.4:

- bench_connection_tracking: update 1000 connections
- bench_fetch_and_normalize: fetch + normalize 1000 connections
- bench_compute_delta: delta computation on 1000 connections
- bench_container_id_extraction: parse cgroup strings
- bench_rate_limiter: 10000 keys

### Labels
testing

## Container image and CI/CD setup

### Type
task

### Priority
2

### Description
Create container packaging and CI pipeline:

- Dockerfile for the Rust collector binary (multi-stage build)
- Multi-arch builds for x86_64, aarch64, s390x, ppc64le
- CI pipeline: build, unit tests, integration tests (where possible)
- Container health check (status-check.sh equivalent)

### Labels
phase-7
