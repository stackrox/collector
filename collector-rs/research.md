# Collector C++ Codebase: Deep Research

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Directory Structure](#2-directory-structure)
3. [Entry Point and Startup Sequence](#3-entry-point-and-startup-sequence)
4. [Core Classes](#4-core-classes)
5. [Falcosecurity-libs Integration](#5-falcosecurity-libs-integration)
6. [Event Processing Pipeline](#6-event-processing-pipeline)
7. [Process Signal Handling](#7-process-signal-handling)
8. [Network Signal Handling](#8-network-signal-handling)
9. [Connection Tracking](#9-connection-tracking)
10. [GRPC Communication](#10-grpc-communication)
11. [Threading Model](#11-threading-model)
12. [Configuration System](#12-configuration-system)
13. [Utilities and Infrastructure](#13-utilities-and-infrastructure)
14. [BPF Driver Integration](#14-bpf-driver-integration)
15. [Container Plugin](#15-container-plugin)
16. [Build System](#16-build-system)
17. [Key Intricacies and Gotchas](#17-key-intricacies-and-gotchas)

---

## 1. Architecture Overview

The collector is an eBPF-based system monitoring agent that captures kernel events (process executions, network connections) from containers and sends them to the StackRox Sensor component via gRPC. It is built on top of **falcosecurity-libs** (a fork at version 0.23.1), which provides the BPF driver, system call capture (libscap), and system inspection (libsinsp) layers.

### High-Level Data Flow

```
Kernel (BPF programs)
    |
    v
libscap (ring buffers, per-CPU)
    |
    v
libsinsp (event parsing, thread table, fd table)
    |
    v
system_inspector::Service (event loop, filtering, dispatch)
    |
    +---> ProcessSignalHandler --> ProcessSignalFormatter --> gRPC --> Sensor
    |
    +---> NetworkSignalHandler --> ConnectionTracker --> NetworkStatusNotifier --> gRPC --> Sensor
```

### Key Design Principles

- **Event-driven**: The main thread blocks on the BPF ring buffer waiting for kernel events
- **Handler pipeline**: Events flow through an ordered list of SignalHandlers, each filtering by event type
- **Delta-based network reporting**: Network connections are tracked in a ConnectionTracker and only deltas (changes) are sent to Sensor
- **Rate limiting**: Process signals are deduplicated via token bucket rate limiting
- **Arena allocation**: Protobuf messages use arena allocation (512KB pools) to minimize GC pressure

---

## 2. Directory Structure

```
collector/
├── collector.cpp              # Main entry point
├── connscrape.cpp             # Network connection scraping utility
├── self-checks.cpp            # Self-check diagnostic binary
├── CMakeLists.txt             # Build configuration
├── Makefile                   # Docker-based build automation
│
├── lib/                       # Core library (~90 source files)
│   ├── CollectorService.*     # Main service orchestrator
│   ├── CollectorConfig.*      # Configuration management
│   ├── CollectorArgs.*        # Command-line argument parsing
│   ├── ConfigLoader.*         # YAML/JSON config file watching (inotify)
│   │
│   ├── ProcessSignalHandler.* # Process event (execve) handling
│   ├── ProcessSignalFormatter.*  # Event-to-protobuf conversion
│   ├── NetworkSignalHandler.* # Network event handling
│   ├── ConnTracker.*          # Connection state tracking & delta computation
│   ├── NetworkConnection.*    # Address, Endpoint, Connection data types
│   ├── NetworkStatusNotifier.*  # Periodic network state reporter
│   ├── NetworkConnectionInfoServiceComm.*  # gRPC for network flows
│   │
│   ├── SignalServiceClient.*  # gRPC stream to Sensor (process signals)
│   ├── DuplexGRPC.h           # Bidirectional gRPC abstraction
│   ├── GRPC.*                 # Channel creation, TLS setup
│   │
│   ├── SignalHandler.h        # Base handler interface
│   ├── ProtoSignalFormatter.h # Base formatter template
│   ├── ProtoAllocator.h       # Arena/heap protobuf allocation
│   ├── StoppableThread.*      # Managed thread with graceful shutdown
│   ├── RateLimit.*            # Token bucket rate limiter
│   ├── NRadix.*               # Radix tree for IP/CIDR matching
│   ├── Hash.h                 # Generic hashing framework
│   ├── Containers.h           # STL container helpers
│   ├── EventMap.h             # Event ID to value mapping
│   ├── Control.h              # RUN/STOP_COLLECTOR enum
│   │
│   ├── ContainerMetadata.*    # Container info from plugin
│   ├── HostInfo.*             # Host system information
│   ├── HostHeuristics.*       # Host detection heuristics
│   ├── ProcfsScraper.*        # /proc filesystem scraping
│   ├── SelfChecks.*           # Driver validation at startup
│   ├── KernelDriver.h         # BPF driver abstraction
│   │
│   ├── CollectorStats.*       # Internal timing/counter metrics
│   ├── CollectorStatsExporter.*  # Prometheus metrics export
│   ├── Logging.*              # Structured logging system
│   ├── Utility.*              # String, path, UUID utilities
│   ├── Inotify.h              # Linux inotify wrapper
│   ├── TimeUtil.h             # Timestamp utilities
│   │
│   └── system-inspector/      # Falcosecurity-libs bridge
│       ├── Service.*          # sinsp management, event loop
│       ├── EventExtractor.*   # Macro-based field extraction
│       └── SystemInspector.h  # Abstract interface
│
├── proto/                     # Protobuf definitions (symlinks to StackRox protos)
│   ├── api/v1/                # Signal, empty
│   ├── internalapi/sensor/    # Collector, network_connection, signal_iservice
│   └── storage/               # network_flow, process_indicator
│
├── test/                      # Unit tests (~18 files)
│   ├── ConnTrackerTest.cpp    # 80KB - comprehensive connection tests
│   ├── ProcessSignalFormatterTest.cpp  # Process formatting tests
│   └── ...
│
└── container/                 # Docker packaging
    ├── Dockerfile
    └── bin/                   # Built executables
```

### Falcosecurity-libs (top-level)

```
falcosecurity-libs/
├── userspace/
│   ├── libsinsp/              # System inspection library
│   │   ├── sinsp.h            # Main inspector class
│   │   ├── parsers.cpp        # Event parsing (execve, clone, socket ops)
│   │   ├── event.h            # sinsp_evt, sinsp_evt_param
│   │   ├── thread_manager.h   # Thread table management
│   │   ├── plugin.h           # Plugin loading system
│   │   ├── sinsp_filtercheck.h  # Base filtercheck class
│   │   └── sinsp_filtercheck_*.h  # Field extractors (thread, event, fd)
│   │
│   └── libscap/               # System call capture
│       └── engine/
│           ├── modern_bpf/    # Modern BPF engine (primary)
│           ├── kmod/          # Kernel module (legacy)
│           └── nodriver/      # /proc fallback
│
└── driver/
    ├── modern_bpf/programs/   # BPF programs
    │   ├── attached/events/   # Tracepoint handlers (exec, exit, fork)
    │   └── tail_called/events/  # Syscall handlers (connect, bind, etc.)
    ├── event_table.c          # Event parameter definitions
    └── ppm_events_public.h    # Event type enumeration
```

---

## 3. Entry Point and Startup Sequence

**File**: `collector/collector.cpp`

The `main()` function orchestrates a careful initialization sequence:

```
main()
├── HostInfo::Init()                    # Detect OS, kernel, architecture
├── CollectorArgs::getInstance().parse()  # Parse command-line args
├── CollectorConfig::InitCollectorConfig()  # Build configuration
├── ConfigLoader::LoadConfiguration()   # Read YAML config file
├── Set core dump limits (RLIMIT_CORE)
├── Register signal handlers:
│   ├── SIGABRT, SIGSEGV → AbortHandler  # Crash reporting
│   └── SIGTERM, SIGINT → ShutdownHandler  # Graceful shutdown via g_control
├── TLS configuration (if certs available)
├── createChannel() → gRPC channel to Sensor
├── attemptGRPCConnection() → Wait 30s for channel ready
└── RunService()
    └── CollectorService(config, channel, ...)
        ├── system_inspector::Service() init
        │   ├── sinsp creation & configuration
        │   ├── Container plugin load (libcontainer.so)
        │   ├── EventExtractor initialization
        │   ├── Container filter: "container.id != 'host'"
        │   ├── Add SelfCheckHandlers (process + network)
        │   ├── Add ProcessSignalHandler
        │   └── Add NetworkSignalHandler
        ├── ConnectionTracker setup
        ├── NetworkStatusNotifier setup
        ├── CivetServer (HTTP endpoints: status, loglevel, profiler)
        ├── Prometheus exposer
        └── InitKernel() → KernelDriverCOREEBPF::Setup()
            └── inspector->open_modern_bpf(buffer_size, cpus, syscalls)
        └── RunForever()
            ├── ConfigLoader::Start()          # inotify thread
            ├── NetworkStatusNotifier::Start()  # Periodic scrape thread
            ├── StatsExporter::start()          # Metrics thread
            ├── system_inspector_.Start()
            │   ├── All handlers Start()
            │   ├── inspector_->start_capture()  # Begin BPF capture
            │   └── Detach self-check thread
            └── Loop: system_inspector_.Run(*control_)
                until control_ == STOP_COLLECTOR
```

### Signal Handling for Shutdown

A global `std::atomic<ControlValue> g_control` is used for cross-thread shutdown coordination. The SIGTERM/SIGINT handler sets `g_control = STOP_COLLECTOR`, which causes the main event loop to exit cleanly.

---

## 4. Core Classes

### CollectorService (`lib/CollectorService.h/cpp`)

The main orchestrator that owns and manages all collector components:

- Creates and wires together all subsystems in its constructor
- `InitKernel()` loads the BPF driver
- `RunForever()` is the main lifecycle method: starts all background threads, then enters the sinsp event loop
- HTTP management endpoints via CivetServer:
  - Status endpoint
  - Log level control
  - Profiler control (gperftools)
  - Introspection endpoint

### CollectorConfig (`lib/CollectorConfig.h/cpp`)

Central configuration holder with thread-safe access via `std::shared_mutex`:

**Key configuration values:**
- `kScrapeInterval = 30` seconds (network connection scraping)
- `kMaxConnectionsPerMinute = 2048`
- `kCollectionMethod = CORE_BPF` (only modern BPF supported now)
- Syscall list: execve, clone, fork, vfork, socket, connect, accept, accept4, close, shutdown, listen, bind, getsockopt, sendto, sendmsg, sendmmsg, recvfrom, recvmsg, recvmmsg
- Buffer size: 512MB total sinsp ring buffer
- Thread cache: 32768 entries, 30s timeout
- Afterglow: enabled, 300s period

**Runtime updates**: Configuration can be updated at runtime via `SetRuntimeConfig()`, which is called by the ConfigLoader when the config file changes (watched via inotify).

### system_inspector::Service (`lib/system-inspector/Service.h/cpp`)

The core event processing engine that bridges collector with falcosecurity-libs:

- Owns the `sinsp` instance (with mutex protection - sinsp is not thread-safe)
- Manages the ordered list of SignalHandlers with per-handler event type filters
- Implements the `GetNext()` → filter → dispatch loop
- Handles `SendExistingProcesses()` for handler initialization (replays thread table)
- Maintains per-event-type statistics (event counts, timing)

---

## 5. Falcosecurity-libs Integration

### sinsp Instance Configuration

The collector creates and configures sinsp in `Service::Service()`:

```cpp
sinsp* inspector = new sinsp(metrics_enabled);
inspector->set_snaplen(0);                    // Disable payload capture
inspector->set_import_users(config.ImportUsers());
inspector->m_thread_manager->set_max_thread_table_size(config.GetSinspThreadCacheSize());
inspector->set_thread_timeout_s(30);          // 30s idle timeout
inspector->set_thread_purge_interval_s(60);   // 60s purge sweep
inspector->set_auto_threads_purging(true);
```

### Filter System

The collector uses sinsp's filter system to exclude host events:

```cpp
// Must use custom filter factory because default doesn't support plugin fields
auto filter_factory = sinsp_filter_factory(inspector, plugin_filterlist);
auto filter = sinsp::compile_filter("container.id != 'host'", filter_factory);
inspector->set_filter(std::move(filter));
```

This filter is critical: it ensures only container events reach the collector, excluding all host-level activity.

### Thread Table Access

sinsp maintains a thread table (`m_thread_manager`) that maps TID to `sinsp_threadinfo`. The collector accesses this for:

1. **Process lineage** - Walking parent chain via `tinfo->m_ptid`
2. **Existing process enumeration** - `threads->loop()` to iterate all known processes
3. **Container ID lookup** - Via threadinfo's dynamic field
4. **Process metadata** - exe, exepath, comm, args, uid, gid, cwd

### Event Parameter Access

Events carry parameters defined in `driver/event_table.c`. For example, `PPME_SYSCALL_EXECVE_19_X` has 31 parameters (0-indexed):
- 0: res (return code)
- 1: exe (executable name)
- 2: args (argument list)
- 3: tid, 4: pid, 5: ptid
- 6: cwd
- 13: comm
- 14: cgroups
- 26: uid
- 27: trusted_exepath
- 30: filename (bprm->filename - used for exepath fallback)

---

## 6. Event Processing Pipeline

### Main Event Loop (`Service::Run`)

```cpp
while (control == RUN) {
    ServePendingProcessRequests();  // Handle async process info lookups

    sinsp_evt* evt = GetNext();    // Blocks on BPF ring buffer
    if (!evt) continue;

    // Global filtering
    if (evt->get_category() == EC_INTERNAL) continue;
    if (rhel7_userspace_filter_needed) { /* extra filtering */ }

    // Dispatch to handlers
    for (auto& handler_entry : signal_handlers_) {
        if (!handler_entry.event_filter[evt->get_type()]) continue;

        auto result = handler_entry.handler->HandleSignal(evt);
        switch (result) {
            case NEEDS_REFRESH: SendExistingProcesses(handler); break;
            case FINISHED: /* remove handler */ break;
        }
    }
}
```

### GetNext() Details

```cpp
sinsp_evt* Service::GetNext() {
    std::lock_guard<std::mutex> lock(libsinsp_mutex_);

    sinsp_evt* event = nullptr;
    int32_t res = inspector_->next(&event);

    // Record timing statistics
    userspace_stats_.event_parse_micros[event->get_type()] += elapsed;
    userspace_stats_.nUserspaceEvents[event->get_type()]++;

    // Filter internal events and runc containers
    if (FilterEvent(event)) {
        userspace_stats_.nFilteredEvents[event->get_type()]++;
        return nullptr;
    }

    return event;
}
```

### Event Filtering

Two levels of filtering:
1. **sinsp-level**: `"container.id != 'host'"` filter compiled into sinsp (excludes host events before they reach collector)
2. **Collector-level**: `FilterEvent()` in Service.cpp excludes:
   - runc processes (container runtime internals)
   - Events from `/proc/self` paths (self-monitoring)

### Handler Registration

Each SignalHandler declares its relevant events:
```cpp
// ProcessSignalHandler
std::vector<std::string> GetRelevantEvents() { return {"execve<"}; }

// NetworkSignalHandler (simplified)
std::vector<std::string> GetRelevantEvents() {
    return {"connect<", "accept<", "accept4<", "close<", "shutdown<",
            "getsockopt<", "sendto<", "sendto>", "recvfrom<", ...};
}
```

The Service converts these to a `std::bitset<PPM_EVENT_MAX>` per handler for O(1) event type filtering.

---

## 7. Process Signal Handling

### ProcessSignalHandler (`lib/ProcessSignalHandler.h/cpp`)

Only handles `execve<` events (process execution exit events).

**Flow:**
1. Receive `sinsp_evt*` from event loop
2. Call `ProcessSignalFormatter::ToProtoMessage(evt)` to convert to protobuf
3. Compute deduplication key: `(container_id, name, args[0:256], exec_file_path)`
4. Check `RateLimitCache` (10 bursts per 30-minute window per unique key)
5. If not rate-limited, call `client_->PushSignals()` to send via gRPC

**Statistics tracked:**
- `nProcessSent` - Successfully sent signals
- `nProcessRateLimitCount` - Dropped due to rate limiting
- `nProcessResolutionFailuresByEvt` - Failed event formatting
- `nProcessSendFailures` - gRPC send failures

### ProcessSignalFormatter (`lib/ProcessSignalFormatter.h/cpp`)

Converts raw sinsp events to `storage::ProcessSignal` protobuf messages.

**Field extraction from events:**
```cpp
const char* container_id = event_extractor_->get_container_id(event);  // From plugin
const char* name = event_extractor_->get_comm(event);                   // m_comm
const char* exepath = event_extractor_->get_exepath(event);             // m_exepath
const int64_t* pid = event_extractor_->get_pid(event);                  // m_pid
const uint32_t* uid = event_extractor_->get_uid(event);                 // m_uid
const uint32_t* gid = event_extractor_->get_gid(event);                // m_gid
const char* args = event_extractor_->get_proc_args(event);              // proc.args
```

**Process Lineage Collection (`GetProcessLineage`):**
- Traverses parent process chain via thread manager
- Stops at container boundaries (checks `m_vpid`)
- Collapses consecutive parents with same `m_exepath` (deduplication)
- Maximum 10 ancestors
- Stores `parent_exec_file_path` and `parent_uid` per ancestor

**Existing Process Handling:**
When the gRPC stream first connects (or reconnects), `NEEDS_REFRESH` triggers `SendExistingProcesses()`, which iterates the sinsp thread table and calls `HandleExistingProcess(sinsp_threadinfo*)` for each. These are marked with `scraped=true` in the protobuf to distinguish from real-time events.

**UTF-8 Sanitization:**
All string fields are run through `SanitizedUTF8()` to replace invalid byte sequences before sending to Sensor.

---

## 8. Network Signal Handling

### NetworkSignalHandler (`lib/NetworkSignalHandler.h/cpp`)

Monitors socket lifecycle events and updates the ConnectionTracker.

**Event mapping:**
- **ADD** (connection established): `connect<`, `accept<`, `accept4<`, `getsockopt<`, and optionally send/recv events
- **REMOVE** (connection closed): `close<`, `shutdown<`

**Connection extraction (`GetConnection`):**
1. Get `sinsp_fdinfo` from the event's file descriptor
2. Validate socket type and state (skip failed/pending unless send/recv)
3. Check event result code (`rawres >= 0`)
4. Determine client/server role from `fd_info->is_role_*`
5. Extract IPv4/IPv6 addresses and ports from socket tuple
6. Get container ID from event
7. Validate connection is relevant (not loopback)
8. Return `Connection(container_id, local, remote, l4proto, is_server)`

**Connection status tracking** (optional, controlled by `collect_connection_status_`):
- Skips failed sockets for connect/accept (but not for send/recv which can be unreliable)
- Skips pending async connections
- Enables more accurate connection lifecycle tracking

**Send/Recv tracking** (optional, controlled by `track_send_recv_`):
- Usually disabled for performance
- When enabled, send/recv events update connection timestamps
- Useful for detecting connections that were established before collector started

---

## 9. Connection Tracking

### ConnectionTracker (`lib/ConnTracker.h/cpp`)

The most complex component (~400+ lines). Maintains a complete view of network connection state with sophisticated delta computation.

### Data Structures

**ConnStatus** - Packed into a single `uint64_t`:
- Upper bit: active flag
- Lower 63 bits: last seen timestamp (microseconds)
- Methods: `IsActive()`, `LastActiveTime()`, `WasRecentlyActive()`, `IsInAfterglowPeriod()`

**ConnMap** - `UnorderedMap<Connection, ConnStatus>`: Raw connection states
**AdvertisedEndpointMap** - `UnorderedMap<ContainerEndpoint, ConnStatus>`: Listen endpoints

### Core Operations

1. **UpdateConnection(conn, timestamp, is_add)**: Called by NetworkSignalHandler for each event. Creates/updates `ConnStatus` in the map.

2. **Update(connections, endpoints, timestamp)**: Called by ProcfsScraper for bulk snapshot updates from /proc. Marks all existing as inactive, then marks scraped ones as active.

3. **FetchConnState(normalize, clear_inactive)**: Thread-safe snapshot of connection state with:
   - **Normalization**: Servers aggregate by port (remote cleared to CIDR); clients aggregate by remote IP:port (local cleared)
   - **Filtering**: Applies ignored ports and network exclusions

4. **ComputeDelta / ComputeDeltaAfterglow**: Computes what changed between two snapshots. Afterglow variant treats recently-active connections as still active to reduce chatter.

### Normalization Logic

Connection normalization is critical for reducing the volume of reported connections:

- **Server connections**: Remote address normalized to network CIDR, remote port cleared. This groups all clients connecting to the same server port.
- **Client connections**: Local endpoint cleared, remote port preserved. This groups by destination service.
- **UDP special handling**: Infers server/client role from ephemeral port heuristics when the role is ambiguous.
- **External IPs config**: Controls whether external (public) IPs are aggregated or reported individually.

### NRadix Tree (`lib/NRadix.h`)

Binary radix tree (based on NGINX's implementation) for efficient O(bit-depth) IP/CIDR membership testing. Used by ConnectionTracker for:
- Checking if an IP belongs to a known network
- Determining if connections should be normalized
- Filtering ignored networks

---

## 10. GRPC Communication

### Two Independent GRPC Streams

1. **Signal stream** (`SignalServiceClient`): Process signals (execve events)
2. **Network flow stream** (`NetworkConnectionInfoServiceComm`): Network connection deltas

### SignalServiceClient (`lib/SignalServiceClient.h/cpp`)

Uses `StoppableThread` for background stream management:

```
Background thread:
    EstablishGRPCStream()  # Retry loop
        EstablishGRPCStreamSingle()
            WaitForChannelReady(30s)
            DuplexClient::CreateWithReadsIgnored()  # Bidirectional stream
            WaitUntilStarted(30s)
            stream_active_ = true

Main thread:
    PushSignals(msg)
        if first_write: return NEEDS_REFRESH  # Trigger existing process sync
        stream->Write(msg)
        if write_fails: stream_active_ = false, notify stream_interrupted_
```

**Reconnection**: On write failure, the background thread is notified via `stream_interrupted_` condition variable and re-enters `EstablishGRPCStream()` to reconnect.

**First-write semantics**: The first successful write returns `NEEDS_REFRESH`, which causes the event loop to call `SendExistingProcesses()` - this replays all known processes from the sinsp thread table so Sensor has a complete picture.

### NetworkConnectionInfoServiceComm (`lib/NetworkConnectionInfoServiceComm.h/cpp`)

Bidirectional stream with Sensor's `NetworkConnectionInfoService`:

**Outbound**: `NetworkConnectionInfoMessage` containing delta of connections and endpoints
**Inbound**: `NetworkFlowsControlMessage` with:
- Public IP lists
- Known IP networks
- External IPs configuration
- Afterglow period settings
- Ignored protocol/port pairs

Advertises capabilities: `public-ips,network-graph-external-srcs`

### DuplexGRPC (`lib/DuplexGRPC.h`)

Advanced bidirectional gRPC streaming abstraction:
- Enables simultaneous blocking reads/writes without explicit thread management
- Completion queue-based async coordination
- Poll-style interface (`Poll()`, `PollAny()`, `PollAll()`)
- Timeout support with `gpr_timespec`
- Graceful shutdown with `TryCancel()`, `WritesDone()`, `Finish()`

---

## 11. Threading Model

### Thread Inventory

| Thread | Owner | Purpose |
|--------|-------|---------|
| **Main thread** | `CollectorService::RunForever()` | sinsp event loop: `GetNext()` → filter → dispatch |
| **Signal GRPC thread** | `SignalServiceClient` (StoppableThread) | Maintains gRPC stream to Sensor for process signals |
| **Network GRPC thread** | `NetworkStatusNotifier` (StoppableThread) | Periodic network state reporting |
| **Config loader thread** | `ConfigLoader` (StoppableThread) | Watches config file via inotify |
| **Stats exporter thread** | `CollectorStatsExporter` | Periodic Prometheus metrics publication |
| **Self-check thread** | `Service::Start()` (detached) | Temporary: validates BPF driver at startup |

### Synchronization

- `std::atomic<ControlValue> g_control` - Lock-free shutdown signal between signal handler and main thread
- `std::mutex libsinsp_mutex_` - Protects all sinsp API calls (sinsp is not thread-safe)
- `std::shared_mutex` in CollectorConfig - Concurrent reads, exclusive writes for config updates
- `std::mutex` in ConnectionTracker - Protects connection maps (via `WITH_LOCK()` macro)
- `std::condition_variable` in SignalServiceClient - Coordinates stream lifecycle
- Pipe FDs in StoppableThread - Inter-thread stop signaling

### StoppableThread (`lib/StoppableThread.h/cpp`)

Base class for managed background threads:
- `Start(callable)` - Launch thread with any callable
- `Stop()` - Set atomic flag, write to pipe, join thread
- `Pause(duration)` / `PauseUntil(deadline)` - Interruptible sleep via condition variable
- `should_stop()` - Non-blocking check for stop signal
- `stop_fd()` - File descriptor for poll/select-based stop detection

---

## 12. Configuration System

### Three Configuration Sources

1. **Command-line arguments** (`CollectorArgs`):
   - `--collector-config` (JSON config blob)
   - `--collection-method` (ebpf/core_bpf)
   - `--grpc-server` (Sensor address)

2. **YAML config file** (`ConfigLoader`):
   - Watched via inotify for changes
   - Parsed to `sensor::CollectorConfig` protobuf
   - Applied via `CollectorConfig::SetRuntimeConfig()`

3. **Sensor control messages** (via gRPC):
   - `NetworkFlowsControlMessage` updates network tracking parameters
   - Applied to ConnectionTracker directly

### Environment Variables

Key environment variables used:
- `GRPC_SERVER` - Sensor address
- `COLLECTOR_HOST_ROOT` - Host filesystem mount point (for /proc access)
- `ROX_COLLECTOR_SINSP_TOTAL_BUFFER_SIZE` - Ring buffer size
- `ROX_COLLECTOR_SINSP_THREAD_CACHE_SIZE` - Thread table size
- `ROX_AFTERGLOW_PERIOD` - Connection afterglow duration
- `ROX_COLLECT_CONNECTION_STATUS` - Enable connection status tracking
- `ROX_COLLECTOR_SET_EXTERNAL_IPS` - External IP tracking

---

## 13. Utilities and Infrastructure

### EventExtractor (`lib/system-inspector/EventExtractor.h/cpp`)

Macro-based field extraction system that wraps sinsp's filtercheck API for efficient, type-safe field access:

```cpp
// Direct threadinfo access (fastest)
TINFO_FIELD(comm);      // → tinfo->m_comm
TINFO_FIELD(exepath);   // → tinfo->m_exepath
TINFO_FIELD(pid);       // → tinfo->m_pid

// Plugin/event fields via filtercheck (C string)
FIELD_CSTR(container_id, "container.id");    // From container plugin
FIELD_CSTR(k8s_namespace, "k8s.ns.name");    // From container plugin
FIELD_CSTR(proc_args, "proc.args");          // From sinsp

// Type-safe extraction returning std::optional
FIELD_RAW_SAFE(client_port, "fd.cport", uint16_t);
FIELD_RAW_SAFE(server_port, "fd.sport", uint16_t);
FIELD_RAW_SAFE(event_rawres, "evt.rawres", int64_t);
```

**Initialization**: Each field creates a `sinsp_filter_check` at init time via `new_filter_check_from_fldname()`. The filtercheck is stored and reused for every event extraction.

### Logging (`lib/Logging.h/cpp`)

- Severity levels: FATAL, CRITICAL, ERROR, WARNING, NOTICE, INFO, DEBUG, TRACE
- Macros: `CLOG(LEVEL)`, `CLOG_IF(cond, LEVEL)`, `CLOG_THROTTLED(LEVEL, interval)`
- Output to stderr with timestamp
- Runtime log level changes via HTTP endpoint
- `WriteTerminationLog()` for container shutdown messages

### ProtoAllocator (`lib/ProtoAllocator.h`)

Dual-mode protobuf memory management:
- **Arena mode** (`USE_PROTO_ARENAS`): 512KB pre-allocated pool, grows dynamically, resets between messages
- **Heap mode**: Simple `new` allocation for each message
- Methods: `Allocate<T>()`, `AllocateRoot()`, `Reset()`

### CollectorStats (`lib/CollectorStats.h`)

Singleton metrics collector with:
- 17 timer metrics (net scrape, update, fetch, create, write, process info wait, etc.)
- 24 counter metrics (connection updates, deltas, procfs errors, event anomalies)
- Thread-safe via atomic operations
- Macros: `SCOPED_TIMER()`, `COUNTER_INC()`, `COUNTER_ADD()`, `COUNTER_SET()`

### Hash Framework (`lib/Hash.h`)

Generic hashing using SFINAE to support types with either `Hash()` method or `std::hash<T>` specialization:
- `Hasher` functor, `HashAll()` variadic combiner
- Custom containers: `UnorderedSet<T>`, `UnorderedMap<K,V>` using `Hasher`

### Rate Limiting (`lib/RateLimit.h`)

- `TokenBucket` - Simple state holder
- `TimeLimiter` - Token bucket with configurable burst and refill
- `RateLimitCache` - Key-based rate limiting (used for process signal dedup)
- `CountLimiter` - Simple count-based limiting

---

## 14. BPF Driver Integration

### Driver Selection (`lib/KernelDriver.h`)

Only modern BPF (CO-RE) is used:

```cpp
class KernelDriverCOREEBPF {
    void Setup(CollectorConfig& config, sinsp& inspector) {
        inspector.open_modern_bpf(
            config.GetSinspBufferSize(),
            config.GetSinspCpuPerBuffer(),
            config.GetSyscallList()  // Filtered syscall set
        );
    }
};
```

### Modern BPF Characteristics

- **CO-RE** (Compile Once Run Everywhere): Uses BTF for kernel portability
- **Per-CPU ring buffers**: Avoids global contention for high throughput
- **Tail calls**: Efficient syscall dispatch
- **Exit events only**: Modern BPF drivers no longer send syscall enter events (marked `EF_OLD_VERSION`). Exit events contain all needed information.

### Syscall Filtering

Only configured syscalls are traced to minimize overhead:
```
execve, clone, clone3, fork, vfork,
socket, connect, accept, accept4, close, shutdown,
listen, bind, getsockopt,
sendto, sendmsg, sendmmsg, recvfrom, recvmsg, recvmmsg
```

Plus always-enabled: `PPM_SC_SCHED_PROCESS_EXIT`, `PPM_SC_SCHED_SWITCH` (required for thread table management).

### BPF Program Structure

```
driver/modern_bpf/programs/
├── attached/events/
│   ├── sched_process_exec.bpf.c   # Tracepoint: process exec
│   ├── sched_process_exit.bpf.c   # Tracepoint: process exit
│   ├── sched_process_fork.bpf.c   # Tracepoint: process fork
│   └── sched_switch.bpf.c         # Tracepoint: context switch
│
└── tail_called/events/syscall_dispatched_events/
    ├── connect.bpf.c              # Syscall: connect
    ├── accept.bpf.c               # Syscall: accept
    ├── bind.bpf.c                 # Syscall: bind
    ├── sendto.bpf.c               # Syscall: sendto
    └── ...                        # Other syscall handlers
```

---

## 15. Container Plugin

### Architecture Change in 0.23.1

Container metadata (container.id, k8s.ns.name) was moved from built-in libsinsp fields to a runtime plugin (`libcontainer.so`). This was a major architectural change from earlier versions.

### Plugin Loading (`Service.cpp`)

```cpp
// Load container plugin
auto plugin = inspector_->register_plugin("/usr/local/lib64/libcontainer.so");
plugin->init("", err);

// Register plugin's filtercheck in EventExtractor's custom FilterList
if (plugin->caps() & CAP_EXTRACTION) {
    auto check = sinsp_plugin::new_filtercheck(plugin);
    event_extractor_->FilterList()->add_filter_check(check);
}
```

### Plugin Fields

The container plugin provides:
- `container.id` - Short container ID (12 chars)
- `k8s.ns.name` - Kubernetes namespace

These fields are accessed via the EventExtractor's `FIELD_CSTR` macro, which uses the plugin's filtercheck for extraction.

### Custom Filter Factory

Because the default sinsp filter factory doesn't know about plugin fields, the collector creates a custom `sinsp_filter_factory` with the plugin's FilterList:

```cpp
auto filter_factory = sinsp_filter_factory(inspector_, event_extractor_->FilterList());
auto filter = sinsp::compile_filter("container.id != 'host'", filter_factory);
inspector_->set_filter(std::move(filter));
```

---

## 16. Build System

### CMake Structure

- **Top-level** `CMakeLists.txt`: Enables testing, includes `collector/`
- **collector/** `CMakeLists.txt`: Main build config (C++17, compiler flags, dependencies)
- **collector/lib/** `CMakeLists.txt`: Builds `collector_lib` static library
- **collector/proto/** `CMakeLists.txt`: Generates protobuf/gRPC code

### Build Targets

- `collector_lib` - Static library from all `lib/*.cpp` and `lib/system-inspector/*.cpp`
- `collector` - Main executable linked against `collector_lib`
- `connscrape` - Connection scraping utility
- `self-checks` - Startup validation binary

### Key Dependencies

| Library | Purpose |
|---------|---------|
| falcosecurity-libs (sinsp, scap) | BPF event capture and inspection |
| gRPC++ | Communication with Sensor |
| protobuf | Message serialization |
| yaml-cpp | Configuration file parsing |
| prometheus-cpp | Metrics export |
| civetweb | HTTP server for management endpoints |
| cap-ng | Linux capability management |
| uuid | UUID generation (signal IDs) |
| gperftools | CPU/memory profiling (optional) |
| libbpf | BPF program loading |

### Compiler Flags

```
-std=c++17 -fPIC -Wall -pthread
-Wno-deprecated-declarations
-fno-omit-frame-pointer    # For profiling
-rdynamic                   # For stack traces
```

### Key Defines

- `USE_PROTO_ARENAS` - Enable arena-based protobuf allocation
- `ASSERT_TO_LOG` - Convert asserts to log messages (production)
- `SCAP_SOCKET_ONLY_FD` - Only track socket file descriptors (reduces overhead)
- `INTERESTING_SUBSYS` - CGroup subsystems to monitor

---

## 17. Key Intricacies and Gotchas

### 1. sinsp Is Not Thread-Safe

All sinsp API calls must be under `libsinsp_mutex_`. This includes `next()`, thread table access, and any filtercheck extraction. The main event loop holds this lock during `GetNext()`, and other threads (like async process info lookups) must acquire it too.

### 2. Enter Events Are Gone

Modern BPF drivers no longer send syscall enter events (marked `EF_OLD_VERSION` in event_table.c). All needed information is in the exit event. This caused the exepath bug (see Memory notes): `m_exepath` used to be updated from the enter event's reconstructed path, but since enter events don't exist, exepath never updated from the inherited parent value. The fix uses Parameter 30 (`bprm->filename`) from the exit event as a fallback.

### 3. Container Plugin Required Before EventExtractor Init

The container plugin must be loaded and its filtercheck registered in the EventExtractor's FilterList **before** `EventExtractor::Init()` is called. Otherwise, `new_filter_check_from_fldname("container.id")` will fail because the field doesn't exist in the default filter list.

### 4. First-Write NEEDS_REFRESH Semantics

When the gRPC signal stream first connects, `SignalServiceClient::PushSignals()` returns `NEEDS_REFRESH` on the first write. This triggers `SendExistingProcesses()`, which iterates the sinsp thread table and sends all known processes to Sensor. This ensures Sensor has a complete process inventory even if processes started before the stream was established.

### 5. Afterglow Period Complexity

The afterglow mechanism prevents rapid reporting of connections that frequently connect/disconnect. A connection in the afterglow period is treated as still active even if it's been closed. This interacts with delta computation: `ComputeDeltaAfterglow()` must consider whether a connection was "recently active" within the grace period, not just currently active.

### 6. Connection Normalization is Lossy

Normalization groups connections by aggregating endpoints (clearing local port for clients, clearing remote port/IP for servers). This is intentionally lossy - it reduces the volume of reported connections but loses individual connection granularity. The `NonAggregatedNetworks` configuration can override this for specific networks.

### 7. Rate Limiting Key Truncation

Process signal deduplication keys truncate args to 256 characters. Two processes with identical first 256 chars of args but different suffixes will be treated as duplicates. This is a deliberate trade-off for bounded memory usage.

### 8. Runc Filtering Heuristic

The collector filters out runc events using heuristic checks in `FilterEvent()`. This prevents container runtime internals from being reported as container activity. The filtering is based on process name and path patterns.

### 9. UDP Role Inference

For UDP connections, the server/client role isn't always clear from the socket state. The ConnectionTracker uses ephemeral port heuristics: if a port is in the ephemeral range (typically 32768-60999), it's likely a client. This is a best-effort heuristic and can be wrong for services that bind to high ports.

### 10. Mutex Ordering

When both `libsinsp_mutex_` and ConnectionTracker's internal mutex are needed, `libsinsp_mutex_` must be acquired first to prevent deadlocks. The current code structure generally avoids needing both simultaneously, but this ordering constraint exists implicitly.

### 11. Self-Check Process Validation

On startup, a separate binary (`/usr/local/bin/self-checks`) is executed to verify the BPF driver is working. `SelfCheckProcessHandler` and `SelfCheckNetworkHandler` watch for events from this binary. If no events arrive within 5 seconds, the driver is considered non-functional. These handlers are removed after validation completes (returning `FINISHED`).

### 12. ProtoAllocator Reset Semantics

The arena allocator's `Reset()` doesn't free memory - it resets the allocation pointer to the beginning of the pool. This means the pool grows monotonically during the lifetime of the formatter. A log warning is emitted if the pool exceeds its initial 512KB, indicating potential memory pressure.

### 13. Custom Filter Factory for Plugin Fields

The default `sinsp::set_filter(string)` doesn't support plugin fields because it uses the default filter factory which doesn't include plugin filterchecks. The collector must create a custom `sinsp_filter_factory` with the plugin's FilterList and compile the filter manually. This is a non-obvious integration requirement.
