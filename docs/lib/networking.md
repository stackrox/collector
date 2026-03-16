# Network Flow Tracking

Collector tracks TCP and UDP connections, aggregates them to reduce volume, and sends periodic deltas to Sensor over gRPC. The core flow is: kernel events → ConnTracker → normalization → afterglow → delta → Sensor.

## Architecture

```
CO-RE BPF probes (connect, accept, close, sendto, etc.)
    │
    ▼
NetworkSignalHandler  ← receives syscall events from SystemInspector
    │
    ▼
ConnTracker           ← normalizes, deduplicates, applies afterglow
    │
    ▼
NetworkStatusNotifier ← scrapes /proc/net/tcp as fallback, computes deltas
    │
    ▼
gRPC stream → Sensor  ← NetworkConnectionInfoMessage protos
```

## Event Reception

NetworkSignalHandler (`NetworkSignalHandler.h`) is the abstraction boundary between collector and the kernel instrumentation layer. It receives syscall events and extracts connection metadata.

**Monitored syscalls:** close, shutdown, connect, accept, accept4, getsockopt. When UDP tracking is enabled, also sendto/sendmsg/recvfrom/recvmsg variants.

For each event, the handler:
- Extracts source/destination IPs and ports from the file descriptor info
- Determines connection role (client vs server) from socket flags
- Filters failed or pending async connections
- Builds a Connection object and passes it to ConnTracker

**Key files:** NetworkSignalHandler.cpp (event processing), NetworkConnection.h (Connection type definition)

## Connection Tracking

ConnTracker (`ConnTracker.h`) maintains two state maps:
- **conn_state_** — active connections keyed by Connection (src/dst/port/protocol)
- **endpoint_state_** — listening sockets keyed by ContainerEndpoint

Each entry stores a ConnStatus: a 63-bit microsecond timestamp packed with a 1-bit active flag.

Updates are thread-safe. When a connection event arrives, ConnTracker either inserts it or updates the timestamp if newer. Statistics are tracked by direction (inbound/outbound) and address type (public/private).

**Key files:** ConnTracker.cpp (state management), ConnTracker.h (data structures)

## Normalization

Before transmission, connections are normalized to reduce cardinality:

| Role | Local endpoint | Remote endpoint |
|------|---------------|-----------------|
| Server | Cleared to port only | Address normalized, port set to 0 |
| Client | Entirely cleared | Address + port preserved after normalization |

Address normalization depends on network classification:
- **Private IPs** (RFC1918) — preserved with CIDR from known networks
- **Known public IPs** (cluster nodes) — preserved as /32 or /128
- **Unknown public IPs** — either preserved individually (when external IPs enabled) or collapsed to a sentinel address (255.255.255.255 or ffff:...:ffff)

For UDP, server role detection is unreliable, so ConnTracker compares ephemeral port confidence scores to infer which side is the client.

**Key files:** ConnTracker.cpp `NormalizeConnectionNoLock`, `NormalizeAddressNoLock`

## External IPs

ExternalIPsConfig (`ExternalIPsConfig.h`) controls whether unknown public IPs are preserved or aggregated, configurable per-direction (ingress/egress/both). The config arrives from Sensor via runtime collector config.

When the external IPs setting changes, affected connections must be closed and re-reported in the new format. ConnTracker handles this in `CloseConnectionsOnExternalIPsConfigChange`.

## Network Topology from Sensor

Sensor sends network topology information that ConnTracker uses for normalization:
- **Known public IPs** — cluster node IPs, preserved without aggregation
- **Known IP networks** — CIDR ranges stored in the NRadix tree
- **Ignored L4 pairs** — protocol/port combinations to filter (e.g., metrics endpoints)
- **Ignored networks** — CIDR ranges to completely exclude
- **Non-aggregated networks** — CIDRs where each IP is preserved

These arrive via `NetworkStatusNotifier.OnRecvControlMessage` from the Sensor gRPC stream.

## NRadix Tree

NRadixTree (`NRadix.h`) implements a binary radix tree for fast CIDR lookups, based on the NGINX implementation. It supports both IPv4 and IPv6 with longest-prefix matching — an address matching both /8 and /16 returns the more specific /16.

The tree also supports subset detection via `IsAnyIPNetSubset`, which walks two trees in parallel to determine containment.

**Key files:** NRadix.cpp (insert, find, subset operations)

## Afterglow

Afterglow suppresses transient connection churn by treating recently-closed connections as still active for a configurable window (default 30s). This significantly reduces the volume of open/close pairs sent to Sensor.

The algorithm works during delta computation:
- A connection that closed within the afterglow window is reported as **active**
- Only after the window expires is the **close** event reported
- If the connection reopens before the window expires, no close is ever sent

`ComputeDeltaAfterglow` in ConnTracker.h compares old and new state snapshots, applying these rules to produce the minimal delta.

## Periodic Scraping and Delta Reporting

NetworkStatusNotifier (`NetworkStatusNotifier.h`) runs a periodic loop (default 30s):

1. **Scrape** /proc/net/tcp and /proc/net/tcp6 via ProcfsScraper to catch connections missed by events
2. **Fetch** normalized connection and endpoint state from ConnTracker
3. **Compute delta** against previous state (with or without afterglow)
4. **Build** NetworkConnectionInfoMessage protobuf
5. **Send** over the gRPC stream to Sensor

Active connections are rate-limited per container to prevent flooding. Close events are never rate-limited to avoid orphaned connections in Sensor.

**Key files:** NetworkStatusNotifier.cpp (main loop, delta computation, message building)

## Procfs Fallback

ProcfsScraper (`ProcfsScraper.cpp`) reads /proc to discover connections that existed before collector started or were missed by event processing. It walks /proc looking for containerized processes, reads their network namespaces, parses /proc/PID/net/tcp, and joins socket inodes with file descriptors to build Connection objects.

Listening sockets include an originator process when available. Process objects use lazy resolution — they register a callback with SystemInspector and block up to 30s for process metadata.

**Key files:** ProcfsScraper.cpp (scraping logic), Process.h (lazy resolution)

## gRPC Output

NetworkConnectionInfoServiceComm (`NetworkConnectionInfoServiceComm.h`) manages the bidirectional gRPC stream to Sensor. It advertises capabilities including "public-ips" and "network-graph-external-srcs" via metadata headers.

The stream carries both outbound NetworkConnectionInfoMessages and inbound control messages (IP lists, network configs) from Sensor.

## Debug API

NetworkStatusInspector exposes HTTP endpoints at `/state/network/connection` and `/state/network/endpoint` for debugging connection state. Results can be filtered by container and optionally normalized.
