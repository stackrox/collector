# Process Monitoring

Collector captures process exec events from the kernel and sends them to Sensor as ProcessSignal protobufs. The flow is: kernel exec event → ProcessSignalHandler → ProcessSignalFormatter → gRPC → Sensor.

## Architecture

```
CO-RE BPF probes (execve syscall)
    │
    ▼
ProcessSignalHandler  ← receives exec events from SystemInspector
    │
    ▼
ProcessSignalFormatter ← extracts process metadata, builds protobuf
    │
    ▼
SignalServiceClient → Sensor  ← ProcessSignal messages
```

## Event Reception

ProcessSignalHandler (`ProcessSignalHandler.h`) is the abstraction boundary with the kernel instrumentation layer. It monitors a single syscall: `execve`. Each exec event triggers signal extraction and transmission to Sensor.

For each event, the handler:
1. Calls ProcessSignalFormatter to extract process details and build a protobuf
2. Applies rate limiting — a key is computed from container ID, process name, args (truncated to 256 bytes), and exec path. Repeated identical execs are suppressed.
3. Sends accepted signals to Sensor via SignalServiceClient

The handler also supports `HandleExistingProcess` for processing threadinfo structures from the system inspector, used during initial process discovery at startup.

**Key files:** ProcessSignalHandler.cpp, ProcessSignalHandler.h

## Signal Formatting

ProcessSignalFormatter (`ProcessSignalFormatter.h`) converts kernel events into ProcessSignal protobufs. It handles two input types:
- **sinsp_evt** — live exec events from the BPF probe stream
- **sinsp_threadinfo** — process snapshots from discovery/scraping

For each signal, the formatter extracts:

| Field | Source | Notes |
|-------|--------|-------|
| name | comm or exepath | Falls back to exepath if comm is unavailable |
| exec_file_path | exepath or comm | Reverse fallback from name |
| args | proc_args | Sanitized for UTF-8 validity, optional via config |
| pid, uid, gid | Event or threadinfo | Direct extraction |
| container_id | Event extractor | Maps process to container |
| timestamp | evt->get_ts() or clone_ts | clone_ts for discovered processes |
| lineage | Parent chain walk | Up to 10 ancestors, collapsed duplicates |
| scraped | Boolean | True for discovered (non-exec) processes |

**Process lineage** walks the parent chain using sinsp's `traverse_parent_state`. The traversal stops at container boundaries (checking vpid and container_id) to prevent host process info from leaking into container signals. Consecutive parents with identical exec paths are collapsed, and the chain is capped at 10 entries.

**Key files:** ProcessSignalFormatter.cpp, ProcessSignalFormatter.h

## Fallback Discovery

ProcfsScraper (`ProcfsScraper.h`) provides process discovery by reading /proc when event-based tracking needs supplementation. It reads process metadata from /proc/PID directories, determines container membership from /proc/PID/cgroup, and creates ProcessInfo structures.

This ensures processes that existed before collector started are still reported to Sensor. Discovered signals are marked with `scraped=true` to distinguish them from live exec events.

**Key files:** ProcfsScraper.cpp (scraping), Process.h (lazy resolution with 30s timeout)

## Rate Limiting

Process signals are rate-limited per unique key (container + name + args + path) to prevent flooding Sensor with repetitive process starts. The RateLimitCache tracks recently sent signals and suppresses duplicates within the rate window.

Rate-limited events are counted via `nProcessRateLimitCount` for observability.
