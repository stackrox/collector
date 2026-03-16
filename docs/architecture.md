# StackRox Collector

Runtime data collection agent for StackRox (Red Hat Advanced Cluster Security) platform. Runs on every Kubernetes node, gathering security data from Linux kernel via CO-RE BPF probes.

**Language:** C++ (core) + Go (integration tests)
**Driver:** CO-RE BPF only (kernel >= 5.8 with BTF)
**Communication:** gRPC bidirectional streaming to Sensor
**Architecture:** x86_64, aarch64, ppc64le, s390x

## Documentation Index

- **[architecture.md](architecture.md)** (this file) - Architecture, data collection, communication
- **[lib/README.md](lib/README.md)** - C++ library components index
- **[falcosecurity-libs.md](falcosecurity-libs.md)** - CO-RE BPF driver integration
- **[integration-tests.md](integration-tests.md)** - Test framework (26 suites)
- **[build.md](build.md)** - Build system (CMake, Docker, vcpkg)
- **[deployment.md](deployment.md)** - Ansible playbooks, K8s deployment
- **[ebpf-architecture.md](ebpf-architecture.md)** - CO-RE BPF deep dive

## Architecture

### Components

**Collector Process**
Main binary running in privileged DaemonSet pod. Initializes kernel driver, starts event processing threads, manages gRPC connection to Sensor.

**CO-RE BPF Probes** (falcosecurity-libs)
Kernel instrumentation via modern BPF. Attached to tracepoints (sys_enter, sys_exit, sched_process_exit). Writes events to per-CPU ring buffers. Compile-once-run-everywhere (no kernel headers at runtime).

**SystemInspectorService** (collector/lib/system-inspector/Service.cpp)
Event loop consuming libsinsp inspector. Polls ring buffers, enriches events with container metadata, dispatches to NetworkSignalHandler and ProcessSignalHandler.

**NetworkSignalHandler** (collector/lib/NetworkSignalHandler.cpp)
Processes network syscalls (connect, accept, send, recv, close). Extracts 5-tuples, feeds ConnTracker. Implements afterglow-based deduplication.

**ProcessSignalHandler** (collector/lib/ProcessSignalFormatter.cpp)
Processes lifecycle events (execve, fork, clone, exit). Builds process lineage, formats signals, sends to Sensor.

**ConnTracker** (collector/lib/ConnTracker.cpp)
Afterglow algorithm for network flow aggregation. Maintains active/inactive connection maps, deduplicates by 5-tuple, periodic scrubbing. Sends batches to gRPC.

**ConnScraper** (collector/lib/ConnScraper.cpp)
Scans /proc/net/{tcp,udp} for pre-existing connections. Discovers listening endpoints, enriches with process info via /proc/[pid]/fd/.

**CollectorService** (collector/lib/CollectorService.cpp)
gRPC service implementation. Bidirectional streaming with Sensor, handles PushSignals (outbound) and control commands (inbound).

### Data Flow

```
┌─────────────────────────────────────────────────────────┐
│                  Kernel (CO-RE BPF)                      │
│  Tracepoints: sys_enter, sys_exit, sched_process_exit   │
└─────────────────┬───────────────────────────────────────┘
                  │ Per-CPU ring buffers
                  ↓
┌─────────────────────────────────────────────────────────┐
│           libscap (falcosecurity-libs)                   │
│  Ring buffer polling, event parsing, /proc state         │
└─────────────────┬───────────────────────────────────────┘
                  │
                  ↓
┌─────────────────────────────────────────────────────────┐
│           libsinsp (falcosecurity-libs)                  │
│  Event enrichment, container metadata, filtering         │
└─────────────────┬───────────────────────────────────────┘
                  │
                  ↓
┌─────────────────────────────────────────────────────────┐
│         SystemInspectorService (collector)               │
│  Event loop: inspector->next() → dispatch               │
└────┬────────────────────────────────────┬───────────────┘
     │                                    │
     ↓                                    ↓
┌────────────────────┐        ┌─────────────────────────┐
│NetworkSignalHandler│        │ ProcessSignalHandler     │
│  - Parse syscalls  │        │  - Build lineage         │
│  - Extract tuples  │        │  - Format signals        │
│  - Feed tracker    │        │  - Send gRPC            │
└────┬───────────────┘        └─────────────────────────┘
     │
     ↓
┌────────────────────┐        ┌─────────────────────────┐
│ ConnTracker        │        │     ConnScraper          │
│  - Afterglow       │←───────┤  - /proc/net scan       │
│  - Deduplication   │        │  - Endpoints            │
│  - Batch sends     │        └─────────────────────────┘
└────┬───────────────┘
     │
     ↓
┌────────────────────────────────────────────────────────┐
│             CollectorService (gRPC)                     │
│  Bidirectional streaming to Sensor                     │
└────────────────────────────────────────────────────────┘
```

### Threading

**Main Thread**
Initialization, gRPC server, signal handling.

**SystemInspector Thread**
Runs event loop (`inspector->next()`), dispatches to handlers.

**NetworkSignalHandler Thread**
Consumes network events, processes connections.

**ProcessSignalHandler Thread**
Formats process signals, sends gRPC.

**ConnScraper Thread**
Periodic /proc scanning (default: 15s interval).

**Afterglow Scrubber**
Periodic cleanup of inactive connections.

Synchronization via mutexes in ConnTracker, ProcessStore. Lock-free queues for gRPC sends.

## Data Collection

### Network Monitoring

**Syscalls Tracked**
- Connection lifecycle: connect, accept, accept4, bind, listen
- Data transfer: send, sendto, sendmsg, sendmmsg, recv, recvfrom, recvmsg, recvmmsg
- Socket operations: socket, socketpair, shutdown, close
- Status: getsockopt (async connection status)

**Connection Information**
- 5-tuple: src IP:port, dst IP:port, protocol (TCP/UDP)
- Role: client (initiated) vs server (accepted)
- Timestamps: first seen, last seen, close time
- Byte counts: sent, received
- Container context: ID, pod, namespace
- Process context: PID, name, path, cmdline, lineage

**Afterglow Algorithm**
Deduplicates repeated connections within configurable period (default: 30s). When connection closed, moved to inactive map with expiration timestamp. Re-opened connections extend expiration, avoiding duplicate reports. Scrubber periodically removes expired entries.

**Endpoints**
Listening sockets tracked separately. ConnScraper discovers pre-existing listeners via /proc/net/{tcp,udp}. Enriched with originator process (name, path, args, user) via /proc/[pid]/fd/. Reported as separate signal type.

**Public IP Detection**
HostHeuristics determines if endpoint is cluster-internal or external. Uses configurable public IP ranges, cluster CIDR detection.

### Process Monitoring

**Lifecycle Events**
- Execution: execve, execveat
- Creation: fork, vfork, clone, clone3
- Termination: exit (via sched_process_exit tracepoint)

**Process Information**
- PID, PPID, UID, GID
- Executable path, name
- Command line arguments
- Working directory
- Capabilities
- Cgroup membership
- Container ID, pod, namespace
- Lineage (parent chain)

**Lineage Tracking**
ProcessStore maintains parent-child relationships. Process signals include ancestor chain (process → parent → grandparent → ...). Enables attack path reconstruction.

**Container Metadata**
libsinsp resolves container IDs via container runtime APIs (Docker, containerd, CRI-O, Podman). Extracts pod name, namespace, labels, annotations, image.

### Procfs Scraping

ConnScraper periodically reads /proc/net/{tcp,tcp6,udp,udp6,raw} to discover connections established before collector started. Cross-references /proc/[pid]/fd/ to identify process owners. Populates endpoints with originator info.

Scraper also monitors listening sockets. Detects services opening ports, reports as endpoint signals. Handles ephemeral listeners (e.g., load balancers).

### Event Filtering

libsinsp filter engine applies declarative filters:
```
evt.type=connect and container.id!=host and fd.type in (ipv4, ipv6)
```

Collector configures socket-only FD tracking (`SCAP_SOCKET_ONLY_FD`), reducing memory/CPU. Interesting cgroup subsystems limited to perf_event, cpu, cpuset, memory.

Slim threadinfo mode (`SINSP_SLIM_THREADINFO`) omits env vars, reduces memory ~60%.

## Communication Protocol

### gRPC Service

Collector implements bidirectional streaming gRPC service defined in collector.proto (StackRox central repo):

**PushSignals (outbound)**
Collector → Sensor. Sends batches of signals:
- NetworkConnectionInfo: Connection events
- NetworkEndpointInfo: Listening endpoints
- ProcessSignal: Process lifecycle events

**Control Commands (inbound)**
Sensor → Collector:
- Configuration updates
- Connection scraping requests
- Health checks

**Connection Management**
Automatic reconnection with exponential backoff. TLS mutual authentication. Metadata includes collector version, hostname, container runtime.

### Signal Batching

Signals batched for efficiency. Network connections accumulated in ConnTracker, flushed periodically or when batch size threshold reached. Process signals sent individually or in small batches.

Batching controlled by:
- afterglow period (default: 30s)
- connection stats aggregation window (default: 15s)
- batch size limits

### Message Format

Signals serialized as protobuf. NetworkConnectionInfo includes:
- Connection 5-tuple
- Role (client/server)
- Timestamps
- Byte counts
- Container info
- Process info (if available)

ProcessSignal includes:
- Event type (exec, fork, exit)
- Process info
- Lineage chain
- Container info
- Exec args/env (configurable)

## Configuration

### Environment Variables

**Collection**
- COLLECTION_METHOD: "EBPF" (CO-RE BPF)
- GRPC_SERVER: Sensor address (e.g., sensor.stackrox.svc:443)
- COLLECTOR_CONFIG: JSON/YAML config (overrides runtime config)

**Host Access**
- COLLECTOR_HOST_ROOT: Host filesystem mount point (/host)

**Logging**
- COLLECTOR_LOG_LEVEL: trace, debug, info, warning, error
- COLLECTOR_INTROSPECTION_ENABLE: Enable HTTP introspection endpoints

**Performance**
- SINSP_CPU_PER_BUFFER: CPUs per ring buffer (0 = 1:1)
- SINSP_TOTAL_BUFFER_SIZE: Total ring buffer size (512 MB)
- SINSP_THREAD_CACHE_SIZE: Thread info cache (32768)

### Runtime Configuration

YAML file at /etc/stackrox/runtime_config.yaml (or path in COLLECTOR_CONFIG). Inotify-based hot reload without restart.

**Networking**:
```yaml
networking:
  externalIps:
    enable: true
  connectionStats:
    aggregationWindow: 15s
  afterglow:
    period: 30s
```

**Process Listening**:
```yaml
processesListening:
  enable: true
```

**Scraping**:
```yaml
scrape:
  interval: 15s
  enableConnectionStats: true
```

**TLS**:
```yaml
tlsConfig:
  caCertPath: /var/run/secrets/stackrox.io/certs/ca.pem
  clientCertPath: /var/run/secrets/stackrox.io/certs/cert.pem
  clientKeyPath: /var/run/secrets/stackrox.io/certs/key.pem
```

## Build System

See [build.md](build.md) for details.

**Builder Image**
CentOS Stream 10 container with build tools, third-party dependencies built from source. Multi-arch support (amd64, arm64, ppc64le, s390x).

**Build Process**
1. Pull/build collector-builder image
2. CMake configure (find packages, set flags, configure falcosecurity-libs)
3. Compile C++ sources (parallel, ~8-12 min)
4. Strip binary (Release mode)
5. Build container image (UBI 10 Minimal)

**Quick Build**:
```bash
make start-builder
make collector
make image
```

**Incremental**:
```bash
make collector  # ~2-5 min
make image      # ~30 sec
```

## Testing

See [integration-tests.md](integration-tests.md) for details.

**Framework**
Go-based testify/suite with 26 test suites. Validates collector across platforms, kernels, runtimes.

**Test Categories**
- Process/execution: lifecycle, lineage, symlinks, threads
- Network: connections, endpoints, UDP, async, afterglow
- Procfs: scraping, pre-existing connections, listening ports
- Configuration: runtime reload, startup, log levels
- Performance: benchmarks, profiling, resource usage
- API: introspection endpoints, Prometheus metrics
- Kubernetes: namespaces, ConfigMap reload

**Mock Sensor**
Simulated Sensor gRPC server. Receives signals, provides query APIs. Tests verify expected events received.

**Execution**:
```bash
cd integration-tests
make TestProcessNetwork           # Single suite
make ci-integration-tests         # All tests
make ci-benchmarks               # Performance
```

**Remote Testing**:
```bash
REMOTE_HOST_TYPE=gcloud \
VM_CONFIG=ubuntu.ubuntu-20.04 \
COLLECTOR_TEST=TestProcessNetwork \
ansible-playbook -i dev integration-tests.yml
```

## Deployment

See [deployment.md](deployment.md) for details.

**Kubernetes**
DaemonSet on every node. Privileged container with hostPID, hostNetwork. Mounts /host (host filesystem), /sys (sysfs), /sys/kernel/debug (debugfs for eBPF), /var/run/docker.sock (container introspection).

**Resource Requirements**
- Requests: 50m CPU, 320Mi memory
- Limits: 2 CPU, 2Gi memory

**Security Context**
Privileged required for BPF operations (CAP_BPF, CAP_PERFMON on kernel >= 5.8, otherwise CAP_SYS_ADMIN).

**ConfigMap**
runtime_config.yaml mounted at /etc/collector/. Inotify watches for changes, reloads without restart.

**TLS**
Mutual TLS with Sensor. Certificates from Secret mounted at /var/run/secrets/stackrox.io/certs/.

**DaemonSet Example**:
```yaml
apiVersion: apps/v1
kind: DaemonSet
metadata:
  name: collector
  namespace: stackrox
spec:
  selector:
    matchLabels:
      app: collector
  template:
    spec:
      hostPID: true
      hostNetwork: true
      containers:
      - name: collector
        image: quay.io/stackrox-io/collector:latest
        env:
        - {name: COLLECTION_METHOD, value: "EBPF"}
        - {name: GRPC_SERVER, value: "sensor.stackrox.svc:443"}
        securityContext:
          privileged: true
        volumeMounts:
        - {name: host-root, mountPath: /host, readOnly: true}
        - {name: sys, mountPath: /sys, readOnly: true}
        - {name: certs, mountPath: /var/run/secrets/stackrox.io/certs/}
      volumes:
      - {name: host-root, hostPath: {path: /}}
      - {name: sys, hostPath: {path: /sys}}
      - {name: certs, secret: {secretName: collector-tls}}
```

## C++ Codebase

See [lib/README.md](lib/README.md) for component index.

**Location:** `collector/lib/`
**Lines:** ~16,521 C++ across 108 files

**Key Modules**

Event Processing:
- SystemInspectorService: Main event loop
- EventExtractor: libsinsp wrapper
- SignalHandler: Handler interface

Signal Handlers:
- NetworkSignalHandler: Network event consumer
- ProcessSignalFormatter: Process event formatter
- ProcessSignalHandler: Process handler wrapper

Connection Management:
- ConnTracker: Afterglow aggregation
- ConnScraper: /proc/net scanner
- Afterglow: Generic expiration container
- NetworkConnection: 5-tuple representation

Process Management:
- ProcessStore: Process cache
- ContainerMetadata: Container info extractor

gRPC:
- CollectorService: gRPC implementation
- GRPCUtil: Connection helpers
- DuplexGRPC: Bidirectional wrapper

Configuration:
- CollectorConfig: YAML parser
- HostInfo: Host metadata

Kernel:
- KernelDriver: ModernBPFDriver (CO-RE BPF loader)

Utilities:
- Logging, Utility, StoppableThread

**Code Organization**

Headers: Public interfaces in .h files
Implementation: Logic in .cpp files
Tests: *_test.cpp files with GoogleTest
Mocks: Mock* files for testing

**Build Integration**

collector/lib/CMakeLists.txt defines collector_lib target. Links against:
- libsinsp, libscap (falcosecurity-libs)
- gRPC, protobuf
- yaml-cpp, jsoncpp
- civetweb, prometheus-cpp
- gperftools (optional profiling)

## falcosecurity-libs Integration

See [falcosecurity-libs.md](falcosecurity-libs.md) for details.

**Submodule**
git submodule at falcosecurity-libs/. StackRox fork of upstream Falco libs (github.com/stackrox/falcosecurity-libs).

**Version**
3.21.6. Branch: module-version-2.10.

**Customizations**
- BPF verifier fixes for clang > 19 (ROX-31971)
- Container-Optimized OS compatibility (ROX-24938)
- getsockopt syscall support (ROX-18856)
- UDP connectionless tracking
- PowerPC support (ppc64le)
- Kernel 6.x compatibility

**Architecture Layers**

libsinsp (userspace/libsinsp/):
Event enrichment, container metadata, filter engine. Provides sinsp inspector class. Tracks thread table, FD table, network connections.

libscap (userspace/libscap/):
Ring buffer management, event parsing. Reads per-CPU BPF ring buffers, parses ppm_evt_hdr structures.

Modern BPF Driver (driver/modern_bpf/):
CO-RE BPF programs. Attached to sys_enter, sys_exit, sched_process_exit tracepoints. Built as bpf_probe.skel.h embedded in binary.

**Collector Usage**

SystemInspectorService creates sinsp instance:
```cpp
inspector->open_modern_bpf();
while (running) {
    sinsp_evt* evt = inspector->next(&res);
    // Dispatch to NetworkSignalHandler or ProcessSignalHandler
}
```

NetworkSignalHandler extracts connection tuples via EventExtractor wrapper.

ProcessSignalHandler builds lineage from sinsp thread table.

ContainerMetadata wraps `sinsp::get_container_manager()`.

ModernBPFDriver manages probe lifecycle, references g_syscall_table from libscap.

**Configuration**

From collector/CMakeLists.txt:
```cmake
set(BUILD_LIBSCAP_MODERN_BPF ON)
set(BUILD_DRIVER OFF)  # No kernel module
set(SINSP_SLIM_THREADINFO ON)
set(MODERN_BPF_EXCLUDE_PROGS "^(openat2|ppoll|setsockopt|io_uring_setup|nanosleep)$")

add_definitions(-DSCAP_SOCKET_ONLY_FD)
```

Tunables:
- sinsp_cpu_per_buffer_: CPUs per buffer
- sinsp_total_buffer_size_: Total ring buffer (512 MB)
- sinsp_thread_cache_size_: Thread cache (32768)

## Performance

**Ring Buffer Sizing**
Default 512 MB total. Per-CPU buffers sized automatically or via sinsp_cpu_per_buffer_. Too small causes event drops (SCAP_DROP counter), too large causes memory pressure.

**Event Filtering**
Early kernel-side filtering reduces userspace load. SCAP_SOCKET_ONLY_FD processes socket FDs only. Limited cgroup subsystems tracked.

**Slim Threadinfo**
Reduces memory ~60% by omitting env vars, full FD snapshots. Trade-off: less detail in process signals.

**Afterglow Efficiency**
Avoids reporting duplicate connections. Default 30s period balances accuracy vs bandwidth. Longer period = more deduplication but stale data risk.

**Procfs Scraping**
Default 15s interval. Lower = more current endpoint data, higher CPU. Higher = less overhead, delayed discovery.

**Connection Aggregation**
Batch sends reduce gRPC overhead. Default 15s aggregation window. Configured via connectionStats.aggregationWindow.

## Debugging

**Log Levels**
Set COLLECTOR_LOG_LEVEL=trace for verbose output. Logs to stdout (captured by K8s).

**Introspection Endpoints**
Enable via ROX_COLLECTOR_INTROSPECTION_ENABLE=true:
- /ready: Health check
- /state/network/connection: Active connections
- /state/network/endpoint: Listening endpoints
- /state/log-level: Dynamic log level (GET/POST)
- /metrics: Prometheus metrics

**BPF Debug Mode**
Build with BPF_DEBUG_MODE=ON. Enables bpf_printk() in BPF programs. Logs to /sys/kernel/debug/tracing/trace_pipe.

**Core Dumps**
Set kernel.core_pattern, enable coredumpctl. Debug with gdb using image-dev (unstripped binary).

**gRPC Tracing**
Enable via GRPC_TRACE=all GRPC_VERBOSITY=debug. Logs gRPC internals.

**Performance Profiling**
gperftools integration (x86_64):
- CPU: CPUPROFILE=/tmp/cpu.prof
- Heap: HEAPPROFILE=/tmp/heap.prof

Endpoints (introspection enabled):
- /debug/pprof/profile?seconds=30
- /debug/pprof/heap

**Capture Files**
libscap can record events to .scap files for offline analysis. Useful for reproducing issues.

## Security

**Kernel Access**
Privileged container required. BPF operations need CAP_BPF/CAP_PERFMON (kernel >= 5.8) or CAP_SYS_ADMIN (older kernels).

**Event Data**
Syscall arguments may contain sensitive data (file paths, network addresses, cmdline args). Collector does not filter sensitive data, relies on Sensor/Central for redaction.

**BPF Safety**
Modern BPF verifier ensures programs cannot crash kernel. Probes read-only, cannot modify kernel state.

**Ring Buffer Access**
Restricted to collector process (privileged). Kernel ensures data integrity, prevents tampering.

**TLS**
Mutual TLS with Sensor. Certificates provided via K8s Secret. Validates Sensor identity, encrypts communication.

## Known Limitations

**Kernel Requirements**
CO-RE BPF requires kernel >= 5.8 with BTF. Older kernels unsupported.

**Container Runtime**
Requires Docker socket or CRI runtime access for container metadata. Standalone processes on host have limited context.

**Network Monitoring**
Tracks syscall-level events only. Cannot see kernel-bypassed networking (e.g., DPDK, kernel TLS offload). UDP connectionless traffic limited tracking.

**Process Context**
Short-lived processes may exit before collector reads /proc info. Lineage tracking depends on parent still running.

**Resource Overhead**
Privileged container, kernel instrumentation. Typical overhead: 50m-100m CPU, 320Mi-512Mi memory (varies by workload).

**Event Loss**
Under extreme load (e.g., 100k+ events/sec), ring buffers may overflow. SCAP_DROP counter tracks losses. Increase buffer size or reduce event volume.

## Troubleshooting

**Collector Not Starting**

Check pod events:
```bash
kubectl -n stackrox describe pod collector-xxxxx
```

Check logs:
```bash
kubectl -n stackrox logs collector-xxxxx
```

Common issues:
- Kernel too old (< 5.8): CO-RE BPF unavailable
- BTF missing: Check /sys/kernel/btf/vmlinux
- Permissions: Verify privileged: true, hostPID: true
- gRPC server unreachable: Check GRPC_SERVER, network policies

**Events Not Captured**

Check collector logs for errors. Verify BPF probe loaded:
```bash
bpftool prog list | grep collector
```

Check ring buffer stats:
```bash
curl localhost:8080/state/stats  # If introspection enabled
```

Look for SCAP_DROP counter. If non-zero, increase SINSP_TOTAL_BUFFER_SIZE.

Verify events generated (run test workload, check logs for event counts).

**High Memory Usage**

Check thread cache size (SINSP_THREAD_CACHE_SIZE). Reduce if many processes.

Enable slim threadinfo (SINSP_SLIM_THREADINFO=ON in build).

Reduce ring buffer size if memory pressure (but may increase drops).

**High CPU Usage**

Check event rate (SCAP_EVTS counter in stats). If very high, consider filtering.

Increase procfs scrape interval (scrape.interval).

Reduce connection stats aggregation window (networking.connectionStats.aggregationWindow).

**Connection Deduplication Issues**

Verify afterglow period (networking.afterglow.period). If too short, connections reported multiple times. If too long, stale data.

Check connection logs (COLLECTOR_LOG_LEVEL=debug). Look for afterglow evictions, reinsertions.

**Process Lineage Missing**

Parent process may have exited before collector read /proc. Lineage truncated at missing parent.

Increase thread cache size to retain more process info.

**gRPC Connection Failures**

Check TLS certificates:
```bash
kubectl -n stackrox get secret collector-tls -o yaml
```

Verify ca.pem, cert.pem, key.pem present.

Check Sensor connectivity:
```bash
kubectl -n stackrox exec collector-xxxxx -- curl -k https://sensor.stackrox.svc:443
```

Review gRPC logs (GRPC_TRACE=all).

## Version History

**3.74.x** (2026)
ROX-31971: Clang > 19 support, BPF verifier fixes. CentOS Stream 10 builder migration. Kernel 6.15 compatibility.

**3.21.x** (2024)
falcosecurity-libs 3.21.0 integration. Modern BPF excludes (openat2, ppoll, setsockopt). RHEL 9.3 fixes.

**3.18.x** (2023)
UDP connectionless tracking improvements. PowerPC support (ppc64le). Kernel 6.7 compatibility.

**3.0.x** (2022)
ROX-7482: Migration from sysdig to falcosecurity-libs. CO-RE BPF enablement. Afterglow algorithm introduction.

**2.x** (2021)
Original sysdig-based implementation. Classic eBPF probes.

## Contributing

**Code Style**
C++17 standard. Follow Google C++ Style Guide. Use clang-format (make -C collector format).

**Testing**
Add unit tests (*_test.cpp) for new C++ code. Add integration tests for new features. Verify across platforms (RHEL, Ubuntu, COS).

**Pull Requests**
Fork collector repo. Create feature branch. Add tests, update docs. Submit PR to stackrox/collector.

**Build Locally**:
```bash
make start-builder
make collector CMAKE_BUILD_TYPE=Debug
make image-dev
```

**Run Tests**:
```bash
cd integration-tests
make TestProcessNetwork
```

## Support

**Issues**
File Jira tickets in StackRox project (ROX-xxxxx). Include collector version, kernel version, platform, logs.

**Logs**
Collect via:
```bash
kubectl -n stackrox logs collector-xxxxx > collector.log
```

Enable debug logging (COLLECTOR_LOG_LEVEL=debug).

**Debugging**
Use image-dev for symbols. Attach gdb, analyze core dumps. Use introspection endpoints for live state.

## References

**Documentation**
- [lib/README.md](lib/README.md) - C++ library components
- [falcosecurity-libs.md](falcosecurity-libs.md) - BPF driver details
- [integration-tests.md](integration-tests.md) - Test framework
- [build.md](build.md) - Build system
- [deployment.md](deployment.md) - Deployment automation
- [ebpf-architecture.md](ebpf-architecture.md) - CO-RE BPF deep dive

**External**
- [Falco Documentation](https://falco.org/docs/)
- [eBPF Documentation](https://docs.kernel.org/bpf/)
- [CO-RE (Compile Once - Run Everywhere)](https://nakryiko.com/posts/bpf-portability-and-co-re/)
- [StackRox Documentation](https://docs.stackrox.com/)

**Repositories**
- [Collector](https://github.com/stackrox/collector)
- [falcosecurity-libs fork](https://github.com/stackrox/falcosecurity-libs)
- [Upstream Falco libs](https://github.com/falcosecurity/libs)
