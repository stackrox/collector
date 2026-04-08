# collector/lib/ Index

Core C++ implementation of StackRox Collector runtime data collection engine.

**Codebase:** ~16,521 lines C++ across 108 files
**Location:** `collector/lib/`

## Component Overview

**System Inspector** → Manages libsinsp event loop, dispatches to handlers
**Signal Handlers** → Process/network event consumers, feed gRPC senders
**Connection Tracking** → Afterglow-based aggregation, deduplication
**gRPC Communication** → Bidirectional streaming to Sensor
**Procfs Scraper** → Pre-existing connection discovery
**Configuration** → Runtime reload via inotify

## Key Modules

### Event Processing

`system-inspector/Service.{h,cpp}` - SystemInspectorService
Main event loop. Creates sinsp inspector, opens modern BPF driver, registers NetworkSignalHandler and ProcessSignalHandler, runs `inspector->next()` dispatch loop.

`EventExtractor.{h,cpp}` - system_inspector::EventExtractor
Wraps libsinsp event APIs. Extracts syscall parameters, connection tuples, process info. Used by NetworkSignalHandler.

### Signal Handlers

`NetworkSignalHandler.{h,cpp}` - NetworkSignalHandler
Consumes network syscalls (connect, accept, sendto, recvfrom, close). Extracts tuples, feeds ConnTracker, tracks async connection status.

`ProcessSignalFormatter.{h,cpp}` - ProcessSignalFormatter
Consumes process events (execve, fork, clone, exit). Builds lineage, formats signals, sends via gRPC.

`SignalHandler.h` - ISignalHandler interface
Base class for all handlers. Defines `Start()`, `Stop()`, `Run()` lifecycle.

### Connection Management

`ConnTracker.{h,cpp}` - ConnTracker
Afterglow-based flow aggregation. Maintains active/inactive connection maps, deduplication by 5-tuple, periodic scrubbing. Sends to gRPC queue.

`ConnScraper.{h,cpp}` - ConnScraper
Scans /proc/net/{tcp,udp,raw} for pre-existing connections. Populates endpoints, tracks listen sockets. Enriches with process info via /proc/[pid]/fd/.

`Afterglow.{h,cpp}` - Afterglow<T>
Generic afterglow container. Tracks active/inactive items with expiration. Template used by ConnTracker.

### Network Infrastructure

`NetworkConnection.{h,cpp}` - NetworkConnection
Connection 5-tuple representation (src/dst IP:port, protocol). Equality, hashing for map keys.

`NetworkStatusNotifier.{h,cpp}` - NetworkStatusNotifier
Async connection status tracking via getsockopt. Monitors non-blocking connects.

`HostHeuristics.{h,cpp}` - HostHeuristics
Public IP detection. Determines if connection endpoint is cluster-internal or external.

### Process Management

`ProcessSignalHandler.{h,cpp}` - ProcessSignalHandler
Wraps ProcessSignalFormatter. Implements ISignalHandler interface.

`ProcessStore.{h,cpp}` - ProcessStore
Caches process info. Maps PID → ProcessInfo, handles lineage updates.

`ContainerMetadata.{h,cpp}` - ContainerMetadata, IContainerMetadataExtractor
Extracts container ID, pod name, namespace via libsinsp container manager.

### gRPC Communication

`CollectorService.{h,cpp}` - CollectorService
gRPC service implementation. Bidirectional streaming, handles signals/commands from Sensor.

`GRPCUtil.{h,cpp}` - grpc utilities
Connection management, channel creation, credentials.

`CollectorStats.{h,cpp}` - CollectorStatsExporter
Prometheus metrics export, gRPC endpoint for stats.

### Configuration

`CollectorConfig.{h,cpp}` - CollectorConfig
Parses YAML runtime config. Networking, scraping, afterglow, TLS settings.

`FileDownloader.{h,cpp}` - DummyFileDownloader
Stub for kernel object downloads (deprecated).

### Kernel Interface

`KernelDriver.{h,cpp}` - ModernBPFDriver
Loads CO-RE BPF probe. References libscap engine, manages driver lifecycle.

`SysdigService.{h,cpp}` - UserSysdigService (legacy)
Original falcosecurity-libs wrapper. Being phased out in favor of SystemInspectorService.

### Utilities

`Utility.{h,cpp}` - String, time, networking helpers
`Logging.{h,cpp}` - Logging infrastructure
`HostInfo.{h,cpp}` - Host metadata (OS, kernel version)
`DuplexGRPC.h` - Bidirectional gRPC wrapper
`StoppableThread.h` - RAII thread management
`GRPC.{h,cpp}` - gRPC async infrastructure

### Testing

`CollectorServiceMocks.h` - Mock gRPC services
`MockCollectorConfig.h` - Test configuration
`MockSysdigService.h` - Mock event source

## Data Flow

```
Kernel (CO-RE BPF)
    ↓
libscap ring buffers
    ↓
libsinsp (enrichment)
    ↓
SystemInspectorService::next()
    ↓
    ├→ NetworkSignalHandler
    │   ├→ EventExtractor (parse syscalls)
    │   ├→ ConnTracker (afterglow)
    │   └→ CollectorService (gRPC send)
    │
    └→ ProcessSignalHandler
        ├→ ProcessSignalFormatter (build signals)
        └→ CollectorService (gRPC send)

Parallel:
ConnScraper (periodic)
    ↓ /proc/net/{tcp,udp}
ConnTracker (endpoints)
    ↓
CollectorService (gRPC send)
```

## Threading Model

Main thread: Initialization, gRPC server
SystemInspector thread: Event loop (`inspector->next()`)
NetworkSignalHandler thread: Connection processing
ProcessSignalHandler thread: Process signal formatting
ConnScraper thread: Periodic /proc scanning
Afterglow scrubber: Periodic inactive connection cleanup

Synchronization via mutexes in ConnTracker, ProcessStore. Lock-free queues for gRPC sends.

## Configuration

Environment variables: GRPC_SERVER, COLLECTION_METHOD, COLLECTOR_CONFIG (JSON/YAML).
Runtime config: /etc/stackrox/runtime_config.yaml (inotify reload).
Key settings: afterglow period, scrape interval, connection stats aggregation, TLS config.

## Historical Context

ROX-7482 migrated from sysdig to falcosecurity-libs. Afterglow algorithm introduced for network flow deduplication. ConnScraper added for pre-existing endpoint discovery. Async connection tracking via getsockopt (ROX-18856). Process lineage handling evolved through multiple iterations. gRPC bidirectional streaming replaced unary calls.

## Technical Debt

Legacy ConnTracker coexists with ConnTracker. SysdigService being replaced by SystemInspectorService. Multiple process signal paths (ProcessSignalHandler vs ProcessSignalFormatter). Inconsistent error handling patterns. Global state in some utilities.

## References

- [Architecture](../architecture.md)
- [falcosecurity-libs](../falcosecurity-libs.md)
- [Build System](../build.md)
- [Integration Tests](../integration-tests.md)
