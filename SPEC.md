# SPEC.md -- stackrox/collector

> Living specification for the RHACS Collector runtime agent, with particular
> focus on the **collector-drop-privs** effort to replace `privileged: true`
> with a minimal Linux capability set.
>
> **Branch:** `rc-remove-privileged`
> **Last updated:** 2026-06-10

---

## 1. Project Overview

Collector is a C++ eBPF-based runtime security agent that forms part of the
Red Hat Advanced Cluster Security for Kubernetes (RHACS / StackRox) platform.
It runs as a **DaemonSet on every node** in a Kubernetes cluster, capturing
low-level operating system events and forwarding them to the StackRox Sensor
component for policy evaluation and alerting.

### Core Responsibilities

| Responsibility | Mechanism |
|---|---|
| **Process event collection** | Monitors `execve` syscall exit events via BPF tracepoints. Formats each event into a `ProcessSignal` protobuf and streams it to Sensor over a gRPC `SignalService.PushSignals` client-streaming RPC. |
| **Network flow tracking** | Dual-source approach: (1) real-time BPF events for `connect`, `accept`, `close`, `shutdown`, `getsockopt`, `sendto`/`recvfrom`; (2) periodic `/proc/net/tcp{,6}` scraping every 30 s. Merged in `ConnectionTracker` with afterglow (5 min grace period). Sent to Sensor over a bidirectional `NetworkConnectionInfoService.PushNetworkConnectionInfo` stream. |
| **Container lifecycle** | Identifies containers by parsing `/proc/<pid>/cgroup` for 64-char hex IDs. Filters events to containerized processes only (`container.id != host`). Supports CRI, CRI-O, containerd, and optionally Docker/Podman container engines via falcosecurity-libs. |
| **Endpoint detection** | Identifies listening sockets per container via `/proc` fd scanning and BPF `bind`/`listen` events. Reports to Sensor as `ListeningEndpoint` messages. |
| **Health and observability** | HTTP health API on port 8080 (`/ready`), Prometheus metrics on port 9090, gperftools CPU/heap profiling, introspection REST API. |

### Communication Model

```
  Kernel
    |
    | BPF ring buffers (BPF_MAP_TYPE_RINGBUF)
    v
  Collector (C++ binary, per-node DaemonSet)
    |
    | gRPC over mTLS (cert rotation via FileWatcherCertificateProvider)
    | SNI hostname: sensor.stackrox (configurable)
    | Two streaming RPCs:
    |   1. SignalService.PushSignals (client-streaming)
    |   2. NetworkConnectionInfoService.PushNetworkConnectionInfo (bidi)
    v
  Sensor (Go binary, per-cluster Deployment)
    |
    v
  Central (policy engine, UI, API)
```

Collector can also run in **standalone mode** (empty `GRPC_SERVER` env var),
outputting all gRPC messages as JSON to stdout.

---

## 2. Architecture

### Component Diagram

```
+-----------------------------------------------------------------------+
|  Kernel                                                                |
|                                                                        |
|  BPF Programs (tp_btf/sys_enter, tp_btf/sys_exit,                     |
|    sched_process_exec, sched_process_fork, sched_process_exit, ...)    |
|         |                                                              |
|   BPF_MAP_TYPE_RINGBUF (per-CPU, in BPF_MAP_TYPE_ARRAY_OF_MAPS)       |
+---------|-------------------------------------------------------------+
          | mmap'd ring buffers
          v
+-----------------------------------------------------------------------+
|  Userspace: collector binary                                           |
|                                                                        |
|  +--------------------+  +-------------------+  +------------------+   |
|  | falcosecurity-libs |  | collector C++     |  | HTTP / Metrics   |   |
|  |                    |  |                   |  |                  |   |
|  | libpman: BPF skel  |  | CollectorService  |  | CivetWeb :8080   |   |
|  |   lifecycle mgmt   |  |   orchestrator    |  | Prometheus :9090 |   |
|  |                    |  |                   |  |                  |   |
|  | libscap: event     |  | system_inspector  |  | ConfigLoader     |   |
|  |   capture engine   |  |   ::Service       |  |   (inotify)      |   |
|  |                    |  |   event loop      |  |                  |   |
|  | libsinsp: event    |  |                   |  +------------------+   |
|  |   enrichment +     |  | SignalHandlers:   |                         |
|  |   process/net      |  |  ProcessSignal    |  +------------------+   |
|  |   state tracking   |  |  NetworkSignal    |  | gRPC Clients     |   |
|  +--------------------+  |  SelfCheck        |  |                  |   |
|                          |                   |  | SignalService    |   |
|  +--------------------+  | ConnectionTracker |  | NetworkConnInfo  |   |
|  | ProcfsScraper      |  |   + afterglow     |  | Service          |   |
|  |  /host/proc/...    |  |                   |  +------------------+   |
|  +--------------------+  +-------------------+                         |
+-----------------------------------------------------------------------+
```

### Key Subsystems

**BPF Engine (falcosecurity-libs / libpman):**
- CO-RE BPF programs compiled at build time, embedded in the binary as a
  libbpf skeleton (`bpf_probe.skel.h`).
- 9 directly-attached programs: `sys_enter`, `sys_exit`,
  `sched_process_exec`, `sched_process_fork`, `sched_process_exit`,
  `sched_switch`, `page_fault_user`, `page_fault_kernel`, `signal_deliver`.
- 120+ tail-called programs for individual syscall enter/exit handling.
- Ring buffer data transport: per-CPU `BPF_MAP_TYPE_RINGBUF` in a
  `BPF_MAP_TYPE_ARRAY_OF_MAPS`. Default 8 MB per buffer, 512 MB total cap.

**Event Pipeline:**
- `sinsp::next()` reads events from ring buffers in timestamp order.
- Events dispatched through `SignalHandler` chain: `SelfCheckProcessHandler`,
  `SelfCheckNetworkHandler` (startup only, auto-remove after verification),
  `ProcessSignalHandler`, `NetworkSignalHandler`.
- `ProcessSignalFormatter` extracts process metadata (name, exe, args, PID,
  UID, GID, container ID, up to 10 levels of lineage).

**System Inspector (`system_inspector::Service`):**
- Wrapper around falcosecurity-libs `sinsp`.
- `InitKernel()` loads the CO-RE BPF driver via `KernelDriverCOREEBPF`.
- `Run()` calls `sinsp::next()` in a tight loop, filtering and dispatching.

**ProcfsScraper:**
- Periodic `/proc` scraping for network connections.
- Reads `/proc/<pid>/fd/`, `/proc/<pid>/ns/net`, `/proc/<pid>/cgroup`,
  `/proc/<pid>/net/tcp{,6}`, `/proc/<pid>/stat`, `/proc/<pid>/exe`,
  `/proc/<pid>/cmdline`.
- All paths prefixed with `COLLECTOR_HOST_ROOT` (default `/host`) via
  `GetHostPath()` in `collector/lib/Utility.cpp`.

### Data Flow

```
Kernel tracepoints
  -> BPF programs (in-kernel event assembly)
  -> BPF ring buffers (mmap'd to userspace)
  -> libscap engine reads events (lock-free consumer)
  -> libsinsp enriches with process/network/file state
  -> sinsp::next() returns enriched scap_evt
  -> system_inspector::Service dispatches to SignalHandlers
  -> ProcessSignalHandler -> ProcessSignalFormatter -> gRPC PushSignals
  -> NetworkSignalHandler -> ConnectionTracker (merge with /proc scrapes)
  -> NetworkStatusNotifier -> gRPC PushNetworkConnectionInfo
```

### Multi-Architecture Support

| Architecture | BPF target | Kernel vmlinux.h | Status |
|---|---|---|---|
| x86_64 | `x86` | `driver/modern_bpf/definitions/vmlinux.h` | Primary |
| aarch64 | `arm64` | `definitions/vmlinux_aarch64.h` | Supported |
| ppc64le | `powerpc` | `definitions/vmlinux_ppc64le.h` | Supported |
| s390x | `s390` | `definitions/vmlinux_s390x.h` | Supported |

All architectures require kernel >= 5.8 with `CONFIG_DEBUG_INFO_BTF=y`.

---

## 3. Build System

### Builder Container

The builder is a CentOS Stream 10 container
(`quay.io/stackrox-io/collector-builder:master`) providing the complete
toolchain: gcc/g++, clang/LLVM (for BPF), cmake, bpftool, and all
third-party libraries built from source.

**Key files:**
- `builder/Dockerfile` -- builder container definition
- `builder/install/install-dependencies.sh` -- orchestrates numbered install scripts
- `builder/install/versions.sh` -- pinned dependency versions

**Selected dependency versions:**
- libbpf v1.3.4, gRPC v1.81.0, protobuf v33.5
- abseil 20250512.1, TBB 2022.0.0, civetweb v1.16
- googletest v1.15.2, prometheus-cpp v1.2.4

### CMake Build Structure

```
CMakeLists.txt (root)
  collector/CMakeLists.txt
    falcosecurity-libs/ (submodule, builds libsinsp + libscap + modern_bpf)
      driver/modern_bpf/CMakeLists.txt  -- compiles BPF .c -> skeleton
      userspace/libpman/CMakeLists.txt   -- BPF lifecycle manager
      userspace/libscap/                 -- event capture engine
      userspace/libsinsp/                -- event enrichment
    collector/lib/CMakeLists.txt         -- collector_lib static library
    collector/proto/CMakeLists.txt       -- protobuf/gRPC codegen
```

Key CMake variables (`collector/CMakeLists.txt`):
```cmake
BUILD_LIBSCAP_MODERN_BPF=ON
SCAP_HOST_ROOT_ENV_VAR_NAME=COLLECTOR_HOST_ROOT
MODERN_BPF_EXCLUDE_PROGS=openat2,ppoll,setsockopt,io_uring_setup,nanosleep
```

### BPF Program Compilation (CO-RE/BTF)

1. Each `.bpf.c` file in `falcosecurity-libs/driver/modern_bpf/programs/`
   is compiled by clang with `-target bpf -O2` and `__USE_VMLINUX__`.
2. Individual `.bpf.o` files are merged by `bpftool gen object` into a
   single `bpf_probe.o`.
3. `bpftool gen skeleton` produces `bpf_probe.skel.h` -- a C header
   embedding the entire BPF bytecode as a byte array.
4. The skeleton is `#include`d by libpman and compiled into the collector
   binary. No external BPF object files are loaded at runtime.

### Container Image Build

**Production** (`collector/container/Dockerfile`):
- Multi-stage: UBI 10 Micro base + UBI 10 full as package installer.
- Runtime packages: `ca-certificates curl-minimal elfutils-libelf libcap-ng
  libstdc++ libuuid openssl tbb gzip less tar`.
- No `USER` directive -- runs as root (UID 0).
- `ENTRYPOINT ["collector"]`, ports 8080 and 9090 exposed.
- `COLLECTOR_HOST_ROOT=/host` environment variable.

**Development** (`collector/container/dev.Dockerfile`):
- Full CentOS Stream 10 base with sanitizer libraries (ASAN, UBSAN, TSAN).

### Key Build Targets

```bash
make start-builder        # Launch builder container (--cap-add sys_ptrace)
make collector            # cmake configure + build inside builder
make unittest             # Run GTest unit tests inside builder
make image                # Build production runtime container via docker buildx
make teardown-builder     # Stop builder container
```

---

## 4. Kernel Interface & Privilege Surface

This section is the foundation for the drop-privs work. Every privileged
operation is catalogued with its source location and required capability.

### 4.1 BPF Program Loading

**What happens:** At startup, `KernelDriverCOREEBPF::Setup()`
(`collector/lib/KernelDriver.h:69`) calls `inspector.open_modern_bpf()`
which delegates to falcosecurity-libs. The call chain is:

```
sinsp::open_modern_bpf()
  -> scap_modern_bpf__init()          [libscap engine]
     -> pman_init_state()             [libpman/src/configuration.c:95]
        -> setrlimit(RLIMIT_MEMLOCK)  [line 124]   -- CAP_SYS_RESOURCE
     -> pman_open_probe()             [libpman/src/lifecycle.c]
        -> bpf_probe__open()          [libbpf skeleton]
     -> pman_prepare_ringbuf()        [libpman/src/ringbuffer.c]
        -> bpf_map_create(RINGBUF)    [bpf() syscall]   -- CAP_BPF
     -> pman_load_probe()             [libpman/src/lifecycle.c]
        -> bpf_probe__load()          [bpf(BPF_PROG_LOAD)]  -- CAP_BPF
     -> pman_attach_all_programs()    [libpman/src/programs.c]
        -> bpf_program__attach()      [bpf(BPF_LINK_CREATE)] -- CAP_BPF + CAP_PERFMON
```

**Required capabilities:**
- `CAP_BPF` -- `bpf()` syscall for program loading, map creation,
  link creation.
- `CAP_PERFMON` -- Attach BPF programs to kernel tracing infrastructure
  (`BPF_TRACE_RAW_TP` attach type for `tp_btf/` programs).

On kernels < 5.8, these operations require `CAP_SYS_ADMIN` instead.
Since the collector's minimum kernel requirement is >= 5.8 (CO-RE BPF
with `BPF_MAP_TYPE_RINGBUF` and `BPF_PROG_TYPE_TRACING`), the discrete
`CAP_BPF` and `CAP_PERFMON` capabilities are always available.

### 4.2 BPF Capability Probing

**What happens:** At startup, `HostInfo` (`collector/lib/HostInfo.cpp`)
probes BPF capabilities:

```cpp
// line 252
res = libbpf_probe_bpf_map_type(BPF_MAP_TYPE_RINGBUF, NULL);

// line 270
res = libbpf_probe_bpf_prog_type(BPF_PROG_TYPE_TRACING, NULL);
```

These functions attempt to create a test BPF map or load a test BPF program
to verify kernel support. They issue `bpf()` syscalls internally.

`HostHeuristics.cpp` gates startup on these results -- if any returns false,
collector exits with `CLOG(FATAL)`.

**Required capability:** `CAP_BPF`.

### 4.3 RLIMIT_MEMLOCK

**What happens:** `pman_init_state()` in
`falcosecurity-libs/userspace/libpman/src/configuration.c:121-127`
unconditionally bumps `RLIMIT_MEMLOCK` to `RLIM_INFINITY`:

```c
struct rlimit rl = {0};
rl.rlim_max = RLIM_INFINITY;
rl.rlim_cur = rl.rlim_max;
if(setrlimit(RLIMIT_MEMLOCK, &rl)) {
    pman_print_error("unable to bump RLIMIT_MEMLOCK to RLIM_INFINITY");
    return -1;
}
```

**Why:** BPF maps (especially ring buffers) consume kernel-locked memory.
On kernels < 5.11, BPF map memory is charged against `RLIMIT_MEMLOCK`.
On kernels >= 5.11 with cgroup-based BPF memory accounting, this is not
strictly needed, but the library does it unconditionally to handle
backport inconsistencies (Falco issue #2626).

**Required capability:** `CAP_SYS_RESOURCE` -- raising `RLIMIT_MEMLOCK`
`rlim_max` to `RLIM_INFINITY` requires this capability.

Note: collector's own code only calls `setrlimit(RLIMIT_CORE)` in
`collector/collector.cpp:92-106` for core dump configuration. Setting
`RLIMIT_CORE` soft limit does not require `CAP_SYS_RESOURCE` (a process
can set its soft limit up to its hard limit without privileges), but
setting `rlim_max` to `RLIM_INFINITY` does. When `ENABLE_CORE_DUMP=false`
(the K8s default), both limits are set to 0 and no special capability is
needed.

### 4.4 /proc Filesystem Access

**What happens:** `ProcfsScraper` (`collector/lib/ProcfsScraper.cpp`) and
falcosecurity-libs' `scap_procs.c` read host `/proc` entries for
container processes. All access goes through the `/host/proc` hostPath
bind mount, not through PID namespace sharing (`hostPID` is NOT required).

| Path pattern | Purpose | Access method |
|---|---|---|
| `/proc/<pid>/fd/` | Socket inode discovery | `opendir()` + `readlinkat()` |
| `/proc/<pid>/ns/net` | Network namespace identification | `readlinkat()` |
| `/proc/<pid>/cgroup` | Container ID extraction | `open()` + `read()` |
| `/proc/<pid>/net/tcp{,6}` | Connection state scraping | `open()` + `read()` |
| `/proc/<pid>/stat` | Process state | `open()` + `read()` |
| `/proc/<pid>/exe` | Executable path | `readlinkat()` |
| `/proc/<pid>/cmdline` | Process arguments | `open()` + `read()` |
| `/proc/<pid>/environ` | Environment (libsinsp) | `open()` + `read()` |
| `/proc/<pid>/root` | Root filesystem (libsinsp) | `readlinkat()` |
| `/proc/<pid>/loginuid` | Login UID (libsinsp) | `open()` + `read()` |
| `/proc/<pid>/status` | Process status (libsinsp) | `open()` + `read()` |

**Required capability:** `CAP_SYS_PTRACE` -- reading restricted `/proc/<pid>/`
entries for processes in other namespaces (especially `/proc/<pid>/environ`,
`/proc/<pid>/exe` -> actual path, `/proc/<pid>/fd/*`, `/proc/<pid>/root`)
requires this capability or same-user/same-namespace access.

The collector does **not** call `ptrace()` directly. A grep across
`collector/collector/` and `collector/lib/` confirms zero `ptrace()` calls.
The `CAP_SYS_PTRACE` requirement is solely for `/proc` access across
namespace boundaries.

### 4.5 Host Filesystem Access

All host filesystem access uses the `GetHostPath()` function
(`collector/lib/Utility.cpp`) which prepends `COLLECTOR_HOST_ROOT`
(default `/host`):

| Host path | Container path | Purpose | Access |
|---|---|---|---|
| `/proc` | `/host/proc` | Process/network scraping | Read-only |
| `/etc` | `/host/etc` | OS release, hostname | Read-only |
| `/usr/lib` | `/host/usr/lib` | Kernel modules, vmlinux | Read-only |
| `/sys/kernel/debug` | `/host/sys/kernel/debug` | Debug filesystem | Read-only |
| `/sys/kernel/btf/vmlinux` | (direct or via /host) | BTF symbols for CO-RE | Read-only |
| `/sys/firmware/efi` | `/host/sys/firmware/efi` | UEFI/SecureBoot detection | Read-only |

These are all provided via hostPath volume mounts, not by running in the
host filesystem namespace.

### 4.6 Network Listening

| Port | Service | Purpose |
|---|---|---|
| 8080 | CivetWeb HTTP server | Health checks (`/ready`), status, profiling |
| 9090 | Prometheus exporter | Metrics scraping |
| 13037 | Self-checks binary | Startup BPF verification (brief, temporary) |

No special capabilities required for binding to ports > 1024.

### 4.7 fork/execve (Self-Checks)

`SelfChecks.cpp` calls `fork()` + `execve(/usr/local/bin/self-checks)` at
startup to verify the BPF driver captures process and network events.
No special capabilities required.

### 4.8 Other Operations

- **inotify:** `ConfigLoader.cpp` watches `/etc/stackrox/runtime_config.yaml`
  for hot-reload. No special capabilities required.
- **Signal handlers:** SIGTERM, SIGINT, SIGABRT, SIGSEGV handlers registered
  in `collector.cpp`. No special capabilities required.
- **gRPC/TLS:** mTLS with cert rotation. No special capabilities required.

### 4.9 Current vs Target Privilege Model

**Current model:** `privileged: true` in the DaemonSet SecurityContext.
This grants ALL Linux capabilities, disables all MAC restrictions
(SELinux, AppArmor, seccomp), and gives full device access. This is
massively over-provisioned for collector's actual needs.

**Target model:** Four discrete capabilities with all others dropped:

| Capability | Justification | Where exercised |
|---|---|---|
| `CAP_BPF` | Load BPF programs, create BPF maps, create BPF links | `pman_load_probe()`, `pman_prepare_ringbuf()`, `libbpf_probe_*()` |
| `CAP_PERFMON` | Attach BPF programs to kernel tracing infrastructure | `bpf_program__attach()` for `tp_btf/` programs |
| `CAP_SYS_PTRACE` | Read restricted `/proc/<pid>/` entries across namespaces | `ProcfsScraper`, `scap_procs.c` (libsinsp process table init) |
| `CAP_SYS_RESOURCE` | Raise `RLIMIT_MEMLOCK` to `RLIM_INFINITY` for BPF map pinning | `pman_init_state()` in falcosecurity-libs |

**What is NOT needed:**
- `CAP_SYS_ADMIN` -- Not required because kernel >= 5.8 provides `CAP_BPF`
  and `CAP_PERFMON` as discrete capabilities.
- `CAP_NET_ADMIN` -- Collector does not modify network configuration or
  create raw sockets.
- `CAP_DAC_READ_SEARCH` -- Not required because `/proc` access goes through
  hostPath mounts with `CAP_SYS_PTRACE` handling cross-namespace reads.
- `hostPID` -- Not required. Collector reads `/proc` via the hostPath mount,
  not PID namespace sharing.

### 4.10 libcap-ng Status

The collector binary links `libcap-ng` (`cap-ng`) and includes `<cap-ng.h>`
in three files:
- `collector/collector.cpp:18`
- `collector/lib/KernelDriver.h:6`
- `collector/lib/system-inspector/Service.cpp`

However, there are **zero** `capng_*` function calls anywhere in the
collector codebase. The library is linked but not used for any capability
inspection or manipulation. This suggests either planned capability
management code that was never implemented, or remnants of removed code.

---

## 5. Current Security Context

### 5.1 Production Deployment

Collector runs as a DaemonSet in the `stackrox` namespace. The pod spec
is rendered from `collector.yaml.htpl` in the `stackrox/stackrox`
repository. Historically, the SecurityContext is:

```yaml
securityContext:
  privileged: true
```

With host filesystem mounts for `/proc`, `/sys`, `/etc`, `/dev`, `/usr/lib`,
and the container runtime socket, plus `hostNetwork: true` or
`hostPID: true` depending on configuration.

### 5.2 stackrox/stackrox PR (Phase 1 -- Done)

PR [stackrox/stackrox#21065](https://github.com/stackrox/stackrox/pull/21065)
on branch `rc-collector-drop-privs` has landed Phase 1 changes:

**`collector.yaml.htpl` -- collector and fact containers:**
```yaml
securityContext:
  privileged: false
  allowPrivilegeEscalation: false
  capabilities:
    drop: ["ALL"]
    add: ["BPF", "PERFMON", "SYS_PTRACE", "SYS_RESOURCE"]
```

**`collector.yaml.htpl` -- compliance container:**
```yaml
securityContext:
  allowPrivilegeEscalation: false
```

**`collector-pod-security.yaml` (PSP):**
```yaml
privileged: false
hostPID: false
allowedCapabilities:
  - BPF
  - PERFMON
  - SYS_PTRACE
  - SYS_RESOURCE
```

**What is intentionally deferred:**
- OpenShift SCC annotation still points to the built-in `privileged` SCC
  (admission passes while validating capabilities work at the kernel level).
- `node-inventory` container left as `privileged: true` (separate workstream).

### 5.3 Integration Test Harness -- Current State

The integration test framework deploys collector with `Privileged: true`
across all three container runtime paths with NO capability-based
configuration. There is no `CapAdd`, `CapDrop`, or `Capabilities` field
in the `ContainerStartConfig` struct.

**Central configuration struct** (`integration-tests/pkg/config/container_config.go`):
```go
type ContainerStartConfig struct {
    Name        string
    Image       string
    Privileged  bool              // always true for collector
    NetworkMode string
    Mounts      map[string]string
    Env         map[string]string
    Command     []string
    Entrypoint  []string
    Ports       []uint16
    // NO CapAdd, CapDrop, or Capabilities field
}
```

**Where Privileged: true is set for collector:**

1. **Docker/Podman path** -- `integration-tests/pkg/collector/collector_docker.go:126`:
   ```go
   Privileged: true,
   ```

2. **K8s path** -- `integration-tests/pkg/collector/collector_k8s.go:132-139`:
   ```go
   privileged := true
   container := coreV1.Container{
       ...
       SecurityContext: &coreV1.SecurityContext{Privileged: &privileged},
   }
   ```

**Where Privileged flows through to the runtime API:**

3. **Docker executor** -- `integration-tests/pkg/executor/executor_docker_api.go:149-153`:
   ```go
   hostConfig := &container.HostConfig{
       NetworkMode: container.NetworkMode(startConfig.NetworkMode),
       Privileged:  startConfig.Privileged,
       Binds:       binds,
   }
   ```

4. **CRI executor** -- `integration-tests/pkg/executor/executor_cri.go`:
   - Sandbox level (line 181-183): `LinuxSandboxSecurityContext{Privileged: config.Privileged}`
   - Container level (line 235-239): `LinuxContainerSecurityContext{Privileged: config.Privileged}`

### 5.4 Container Runtime Matrix in Tests

| Runtime | Platforms | Executor | Socket |
|---|---|---|---|
| Docker | Ubuntu, COS, Flatcar, Fedora CoreOS | `executor_docker_api.go` | `/var/run/docker.sock` |
| Podman (via Docker-compatible API) | RHEL, RHCOS, SLES, RHEL-SAP | `executor_docker_api.go` | `/run/podman/podman.sock` |
| CRI (containerd) | Garden Linux | `executor_cri.go` | containerd socket |
| Kubernetes (KinD) | GitHub Actions ubuntu-24.04 | `executor_k8s.go` | K8s API |

**Critical note:** Podman on RHEL/RHCOS uses `runtime_as_root: true` and
`needs_selinux_permissive: true` (configured in
`ansible/group_vars/container_engine_podman.yml`). SELinux is explicitly
set to permissive via `sudo setenforce 0` during VM provisioning.

---

## 6. Drop-Privs Implementation Plan

### Phase 1: stackrox/stackrox Template Changes (DONE)

See section 5.2 above. PR #21065 is landed.

### Phase 2: Integration Test Harness Changes (THIS WORK)

Goal: Deploy collector with the capability-based SecurityContext in the
integration test harness, then run the existing test suite to validate
that BPF collection works correctly under the reduced privilege model.

**No new tests are needed.** The existing startup, ring buffer, syscall
event, and network flow tests are sufficient if they pass end-to-end.

#### File 1: `integration-tests/pkg/config/container_config.go`

Add `CapAdd []string` field to `ContainerStartConfig`:

```go
type ContainerStartConfig struct {
    Name        string
    Image       string
    Privileged  bool
    CapAdd      []string   // Linux capabilities to add (used when Privileged is false)
    NetworkMode string
    Mounts      map[string]string
    Env         map[string]string
    Command     []string
    Entrypoint  []string
    Ports       []uint16
}
```

#### File 2: `integration-tests/pkg/executor/executor_docker_api.go`

Wire `CapAdd` into Docker `HostConfig` (around line 149):

```go
hostConfig := &container.HostConfig{
    NetworkMode: container.NetworkMode(startConfig.NetworkMode),
    Privileged:  startConfig.Privileged,
    Binds:       binds,
    CapAdd:      startConfig.CapAdd,   // NEW
}
```

The Docker SDK's `container.HostConfig` already has a `CapAdd` field of
type `strslice.StrSlice` (which is `[]string`). No import changes needed.

#### File 3: `integration-tests/pkg/executor/executor_cri.go`

Wire capabilities into the CRI `LinuxContainerSecurityContext` (around
line 234). Build a `secCtx` variable instead of an inline literal:

```go
secCtx := &pb.LinuxContainerSecurityContext{
    Privileged: config.Privileged,
    NamespaceOptions: &pb.NamespaceOption{
        Network: network,
    },
}
if len(config.CapAdd) > 0 {
    secCtx.Capabilities = &pb.Capability{
        AddCapabilities:  config.CapAdd,
        DropCapabilities: []string{"ALL"},
    }
}
```

The `pb` alias is `k8s.io/cri-api/pkg/apis/runtime/v1` (confirmed from
go.mod at v0.32.2). The type `pb.Capability` has `AddCapabilities []string`
and `DropCapabilities []string` fields.

**Sandbox-level note:** The sandbox-level SecurityContext
(`LinuxSandboxSecurityContext`, line 181) also sets `Privileged`.
Setting `Privileged: false` at the sandbox level while adding capabilities
at the container level is the correct CRI semantic -- the sandbox controls
namespace isolation, the container controls process capabilities.

#### File 4: `integration-tests/pkg/collector/collector_k8s.go`

Replace the hardcoded `privileged: true` SecurityContext (lines 132-139):

```go
// BEFORE:
privileged := true
container := coreV1.Container{
    ...
    SecurityContext: &coreV1.SecurityContext{Privileged: &privileged},
}

// AFTER:
privileged := false
noPrivEsc := false
container := coreV1.Container{
    ...
    SecurityContext: &coreV1.SecurityContext{
        Privileged:               &privileged,
        AllowPrivilegeEscalation: &noPrivEsc,
        Capabilities: &coreV1.Capabilities{
            Drop: []coreV1.Capability{"ALL"},
            Add:  []coreV1.Capability{"BPF", "PERFMON", "SYS_PTRACE", "SYS_RESOURCE"},
        },
    },
}
```

#### File 5: `integration-tests/pkg/collector/collector_docker.go`

Replace `Privileged: true` (line 126) in `createCollectorStartConfig()`:

```go
// BEFORE:
startConfig := config.ContainerStartConfig{
    Name:        "collector",
    Image:       config.Images().CollectorImage(),
    Privileged:  true,
    NetworkMode: "host",
    Mounts:      c.mounts,
    Env:         c.env,
}

// AFTER:
startConfig := config.ContainerStartConfig{
    Name:        "collector",
    Image:       config.Images().CollectorImage(),
    Privileged:  false,
    CapAdd:      []string{"BPF", "PERFMON", "SYS_PTRACE", "SYS_RESOURCE"},
    NetworkMode: "host",
    Mounts:      c.mounts,
    Env:         c.env,
}
```

### Container Runtime Considerations

**Docker seccomp:** Docker's default seccomp profile allows the `bpf()`
syscall since Docker 20.10, but may filter `perf_event_open()` (needed
for `CAP_PERFMON`). If Docker-executor tests fail on Ubuntu/COS VMs but
CRI/podman tests pass on RHEL, the fix is:

```go
hostConfig := &container.HostConfig{
    ...
    SecurityOpt: []string{"seccomp=unconfined"},
}
```

This does NOT affect the CRI/podman path. Podman on RHEL uses the system
seccomp profile, which allows both `bpf()` and `perf_event_open()`
without restriction.

**CRI capabilities:** The `pb.LinuxContainerSecurityContext.Capabilities`
field uses `*pb.Capability` with `AddCapabilities []string` (not
`Add []string` as in K8s core API). Capability names use the kernel
convention without the `CAP_` prefix: `"BPF"`, `"PERFMON"`,
`"SYS_PTRACE"`, `"SYS_RESOURCE"`.

**K8s SecurityContext:** Uses `coreV1.Capabilities` with `Add` and `Drop`
of type `[]coreV1.Capability` (which is `[]string`). Same naming
convention -- no `CAP_` prefix.

### Phase 3: Helm Values, SCC, Operator CRD (FUTURE)

After integration tests pass under the reduced privilege model:

1. Add `collector.privilegedMode` Helm value with conditional
   SecurityContext rendering.
2. Create custom `stackrox-collector` SCC replacing the built-in
   `privileged` SCC binding.
3. Update the `openshift.io/required-scc` annotation to point at the
   custom SCC.
4. Wire through operator CRD if user-facing control is desired.

Plan doc: `stackrox/stackrox/collector-drop-privs-plan.md`.

---

## 7. Testing Architecture

### 7.1 Unit Tests

- **Framework:** Google Test (GTest), 17 test suites
- **Location:** `collector/test/`
- **Build/run:** `make unittest` inside builder container
- **Coverage:** C++ logic only -- string parsing, configuration,
  connection tracking, rate limiting, formatting, cgroup parsing
- **Limitation:** Unit tests CANNOT validate eBPF changes or kernel
  interactions. They test collector's own logic, not falcosecurity-libs
  behavior. CI is required for BPF validation.

### 7.2 Integration Tests

**Framework:** Go + testify, 26 test suites.

**Location:** `integration-tests/`

#### Test VM Provisioning

Ansible provisions GCP VMs across the test matrix:

```
ansible/group_vars/all.yml         -- VM definitions + OS matrix
ansible/roles/provision-vm/        -- OS-specific provisioning
ansible/roles/run-test-target/     -- test execution
ansible/k8s-integration-tests.yml  -- KinD-based K8s tests
```

**VM Matrix:**
| Platform | Arch | Runtime | SELinux |
|---|---|---|---|
| RHCOS OCP 4.12-4.19 | amd64 | Podman | Permissive |
| RHCOS ARM OCP 4.16-4.19 | arm64 | Podman | Permissive |
| Ubuntu 22.04/24.04 | amd64 | Docker | N/A |
| Ubuntu ARM 22.04/24.04 | arm64 | Docker | N/A |
| COS stable/beta/dev | amd64 | Docker | N/A |
| COS ARM | arm64 | Docker | N/A |
| Flatcar stable | amd64 | Docker | N/A |
| Fedora CoreOS stable | amd64 | Docker | Permissive |
| RHEL 8/9/10 | amd64 | Podman | Permissive |
| RHEL ARM 9/10 | arm64 | Podman | Permissive |
| RHEL-SAP (6 variants) | amd64 | Podman | Permissive |
| SLES | amd64 | Podman | N/A |
| Garden Linux | amd64 | CRI/containerd | N/A |
| RHEL s390x 8.6 | s390x | Podman | Permissive |
| RHEL ppc64le 8.8 | ppc64le | Podman | Permissive |

#### Mock Sensor

`integration-tests/pkg/mock_sensor/server.go` -- in-process gRPC server
on port 9999 implementing:
- `SignalService.PushSignals` -- receives process signal events
- `NetworkConnectionInfoService.PushNetworkConnectionInfo` -- bidi
  streaming for network connection and endpoint events

Stores events in maps keyed by container ID. Ring buffer channels
(capacity 32) for live event streaming to test assertions. Filters out
`/proc/self` process signals (known race in falcosecurity-libs).

#### Assertion Helpers

```go
// Process events
sensor.ExpectProcesses(t, containerID, timeout, expected...)
sensor.ExpectProcessesN(t, containerID, timeout, n)
sensor.ExpectLineages(t, containerID, timeout, expected...)

// Network events
sensor.ExpectConnections(t, containerID, timeout, expected...)
sensor.ExpectConnectionsN(t, containerID, timeout, n)
sensor.ExpectSameElementsConnections(t, containerID, timeout, expected...)

// Endpoint events
sensor.ExpectEndpoints(t, containerID, timeout, expected...)
sensor.ExpectEndpointsN(t, containerID, timeout, n)
```

#### Key Test Suites

| Suite | File | Validates |
|---|---|---|
| `CollectorStartupTestSuite` | `collector_startup.go` | Collector starts and stays running |
| `RingBufferTestSuite` | `ringbuf.go` | BPF ring buffer allocation and sizing |
| `PerfEventOpenTestSuite` | `perf_event_open.go` | BPF probes do not block tracepoints |
| `ProcessNetworkTestSuite` | `process_network.go` | Process visibility + TCP network flows |
| `UdpNetworkFlow` | `udp_networkflow.go` | UDP flow detection across syscall variants |
| `RuntimeConfigFileTestSuite` | `runtime_config_file.go` | External IP normalization hot-reload |
| `BenchmarkTestSuiteBase` | `benchmark.go` | Performance under phoronix/berserker loads |
| `K8sTestSuiteBase` | `k8s/base.go` | Kubernetes-specific tests (KinD) |

#### Test Lifecycle

```
1. MockSensor.Start()    -- gRPC server in goroutine
2. Collector.Setup()     -- pull image, merge env/mount/config options
3. Collector.Launch()    -- start container, wait healthy + canary process
4. Test body             -- start workload containers, assert events
5. Collector.TearDown()  -- stop, capture logs, check exit code
6. MockSensor.Stop()     -- stop gRPC, clear event stores
```

#### How to Run

```bash
cd integration-tests

# Full suite (Docker daemon + collector image required)
make ci-integration-tests

# Targeted tests:
go test -v -run TestCollectorStartup
go test -v -run TestRingBuffer
go test -v -run TestPerfEvent

# K8s tests (KinD cluster required):
go test -tags k8s -v -test.run "^TestK8s.*"
```

### 7.3 CI Pipeline

**GitHub Actions** (`/.github/workflows/main.yml`):
```
init -> build-builder-image -> build-collector (4-arch matrix)
  -> build-test-containers -> integration-tests (VM matrix)
                           -> k8s-integration-tests
                           -> benchmarks
```

**Tekton/Konflux** (`.tekton/`):
- Build-only pipeline: multi-arch buildah, Clair scan, ClamAV scan,
  SAST (shell-check, unicode-check, Snyk), RPM signature verification.
- Does NOT run functional/integration tests.

### 7.4 Tests That Validate the Drop-Privs Change

No new tests are needed. The following existing tests serve as
comprehensive regression coverage for the capability-based deployment:

| Test | What it proves |
|---|---|
| `TestCollectorStartup` | Collector starts, health check passes, canary process runs (BPF loaded successfully) |
| `TestRingBuffer` | BPF ring buffer allocated and sized correctly (confirms `CAP_BPF` works for map creation) |
| `TestPerfEvent` | BPF probes attach to tracepoints without blocking (confirms `CAP_PERFMON`) |
| `TestProcessViz` | Process events captured via BPF and delivered to sensor |
| `TestNetworkFlows` | Network connection events from BPF + `/proc` scraping |
| `TestProcessListeningOnPort` | Endpoint detection via BPF and `/proc/fd` scanning (confirms `CAP_SYS_PTRACE` for `/proc` access) |
| `TestProcfsScraper` | Direct `/proc` scraping functionality |
| `TestSocat` | Network connections detected for both client and server |
| `TestUdpNetworkflow` | UDP flow detection across all sendto/recvfrom variants |

---

## 8. Supported Platforms

### Kernel Requirements

- **Minimum:** Linux >= 5.8 with `CONFIG_DEBUG_INFO_BTF=y`
- **Required features:**
  - `BPF_MAP_TYPE_RINGBUF` (since 5.8)
  - `BPF_PROG_TYPE_TRACING` with `BPF_TRACE_RAW_TP` (since 5.8)
  - `/sys/kernel/btf/vmlinux` accessible
- **RHEL backport:** RHEL 8.7+ (kernel 4.18.0-425) includes BTF backpatches;
  however, RHEL 8 is out of scope for the drop-privs work since `CAP_BPF`
  and `CAP_PERFMON` may not be fully functional as discrete capabilities
  on RHEL 8 kernels.

### OS Matrix

| OS Family | Versions Tested | Notes |
|---|---|---|
| RHEL CoreOS (RHCOS) | OCP 4.12 through 4.19 | Primary target, podman runtime |
| RHEL | 8, 9, 10 | Podman runtime |
| RHEL ARM | 9, 10 | Podman runtime, arm64 |
| RHEL SAP | 6 variants | Podman runtime |
| Ubuntu | 22.04, 24.04 | Docker runtime |
| Container-Optimized OS (COS) | stable, beta, dev | Docker runtime |
| Flatcar | stable | Docker runtime |
| Fedora CoreOS | stable | Docker runtime, SELinux permissive |
| SLES | (version TBD) | Podman runtime, no SELinux |
| Garden Linux | latest | CRI/containerd runtime |

### Architecture Support

| Architecture | Primary CI | Container Runtime |
|---|---|---|
| amd64 (x86_64) | Full matrix (15+ VMs) | Docker, Podman, CRI, K8s |
| arm64 (aarch64) | 5 VM types | Docker, Podman |
| s390x | 1 VM type (RHEL 8.6) | Podman |
| ppc64le | 1 VM type (RHEL 8.8) | Podman |

### Container Runtime Support

| Runtime | Engine | Container detection |
|---|---|---|
| CRI-O | OCP default | CRI container engine |
| containerd | K8s default | CRI container engine |
| Docker | Legacy | Docker container engine |
| Podman | RHEL default | Docker-compatible API |

---

## 9. Risk Analysis & Edge Cases

### 9.1 Docker Seccomp Profile and perf_event_open()

**Risk:** Docker's default seccomp profile may block `perf_event_open()`
even when `CAP_PERFMON` is granted. This syscall is needed by
falcosecurity-libs for attaching BPF programs to tracepoints.

**Mitigation:**
- `bpf()` has been in Docker's default seccomp allowlist since Docker 20.10.
- `perf_event_open()` may not be in the allowlist. If Docker-executor tests
  fail but CRI/podman tests pass, add `SecurityOpt: []string{"seccomp=unconfined"}`
  to the Docker executor's `HostConfig`.
- This affects only Ubuntu/COS/Flatcar VMs (Docker runtime). RHEL/RHCOS
  (podman, system seccomp profile) is not affected.

**Impact:** Low -- Docker-path tests are secondary; RHEL/RHCOS is the
primary validation target.

### 9.2 SELinux on RHEL/RHCOS

**Risk:** SELinux enforcing mode may deny BPF operations even with the
correct capabilities if the SELinux policy does not allow the
`bpf { map_create prog_load }` access vectors for the container's
security context.

**Current state:** All podman test VMs set SELinux to permissive via
`needs_selinux_permissive: true` and `sudo setenforce 0`. This means the
test matrix does NOT validate SELinux enforcing behavior.

**Mitigation:**
- The OpenShift SCC annotation still points to the built-in `privileged`
  SCC (intentional for Phase 1), which grants `spc_t` SELinux context --
  this context is allowed to perform BPF operations.
- Phase 3 will create a custom SCC with appropriate SELinux context.
- Production OpenShift clusters use `spc_t` for `privileged` SCC pods;
  the custom SCC must either use `spc_t` or define a policy module that
  allows BPF operations.

**Impact:** Medium -- SELinux policy validation is deferred to Phase 3.

### 9.3 Older Kernel Edge Cases

**Risk:** Some kernels in the 5.8-5.10 range may have inconsistent
`CAP_BPF` / `CAP_PERFMON` behavior, especially vendor-patched kernels.

**Mitigation:**
- RHEL 9 (kernel 5.14+) and OCP 4.12+ (RHCOS with kernel 4.18.0-372+
  with extensive BPF backpatches) are the primary targets.
- The existing BPF capability probing in `HostInfo.cpp`
  (`libbpf_probe_bpf_map_type`, `libbpf_probe_bpf_prog_type`) will
  detect capability failures at startup and exit with `CLOG(FATAL)`.
- COS and Ubuntu VMs in CI run recent kernels (5.15+, 6.x).

**Impact:** Low -- the minimum kernel requirement for CO-RE BPF (>= 5.8)
is already validated at startup.

### 9.4 RLIMIT_MEMLOCK Exhaustion

**Risk:** If `CAP_SYS_RESOURCE` is not granted or `RLIMIT_MEMLOCK`
cannot be raised, BPF map creation will fail with `EPERM` or `ENOMEM`.

**Mitigation:**
- `pman_init_state()` raises `RLIMIT_MEMLOCK` to `RLIM_INFINITY` before
  any BPF operations. If this fails, it returns -1 and collector exits.
- On kernels >= 5.11 with cgroup-based BPF memory accounting,
  `RLIMIT_MEMLOCK` is not used for BPF maps. The raise is done
  unconditionally as a safety measure.
- `CAP_SYS_RESOURCE` is in the target capability set.

**Impact:** Low -- direct failure with clear error message.

### 9.5 node-inventory Container

The `node-inventory` container in the collector DaemonSet remains
`privileged: true`. This is a separate workstream and is NOT affected by
the drop-privs changes. The Phase 1 PR in stackrox/stackrox explicitly
leaves it unchanged.

### 9.6 OpenShift SCC Considerations

**Current state:** The collector DaemonSet uses the built-in `privileged`
SCC via the `openshift.io/required-scc: privileged` annotation.

**Phase 1 (done):** The annotation still points to `privileged` SCC.
This means OpenShift admission will not block the pod even though the
actual SecurityContext requests fewer privileges. The `privileged` SCC
is a superset -- it allows the requested capabilities.

**Phase 3 (future):** Create a custom `stackrox-collector` SCC with:
```yaml
allowPrivilegedContainer: false
allowedCapabilities:
  - BPF
  - PERFMON
  - SYS_PTRACE
  - SYS_RESOURCE
requiredDropCapabilities:
  - ALL
seLinuxContext:
  type: MustRunAs
  seLinuxOptions:
    type: spc_t    # or a custom BPF-capable type
```

### 9.7 Containers That Must Remain Privileged in Tests

Two test-specific container types (not collector itself) use
`Privileged: true` and are NOT affected by this change:

1. **perf-event-open QA container** (`suites/perf_event_open.go:37`) --
   runs `perf_event_open()` to verify collector does not block tracepoints.
2. **Benchmark tool containers** (`suites/benchmark.go:104`) -- perf,
   bpftrace, bcc containers for performance measurement.

These containers serve different purposes (testing/benchmarking tools)
and correctly require full privileges.

---

## 10. Success Criteria

### 10.1 What Passing Tests Prove

If the existing integration test suite passes with the capability-based
SecurityContext:

- **BPF program loading works** with `CAP_BPF` alone (no `CAP_SYS_ADMIN`).
- **BPF tracepoint attachment works** with `CAP_PERFMON` alone.
- **`/proc` scraping works** with `CAP_SYS_PTRACE` via hostPath mount
  (no `hostPID` required).
- **BPF map allocation works** with `CAP_SYS_RESOURCE` for
  `RLIMIT_MEMLOCK` raising.
- **Process events, network flows, and endpoints** are correctly
  captured and delivered to Sensor.
- **Self-check startup verification** passes -- fork/execve + BPF
  event capture.
- **Ring buffer sizing** works correctly under the reduced privilege model.

### 10.2 What Remains to Be Validated in CI

The integration tests run on a subset of the full production matrix.
After the code change, CI validation covers:

| Validation | Coverage |
|---|---|
| Docker runtime + capabilities | Ubuntu, COS, Flatcar VMs |
| Podman runtime + capabilities | RHEL, RHCOS, SLES VMs |
| CRI runtime + capabilities | Garden Linux VMs |
| K8s pod + capabilities | KinD on ubuntu-24.04 |
| Multi-arch (arm64) | RHCOS-arm, Ubuntu-arm, COS-arm |
| Multi-arch (s390x, ppc64le) | RHEL s390x, RHEL ppc64le |
| SELinux enforcing | NOT covered (all VMs use permissive) |
| Production SCC | NOT covered (requires OpenShift cluster) |

### 10.3 Definition of Done -- Each Phase

**Phase 1 (DONE):**
- [x] `collector.yaml.htpl` SecurityContext updated
- [x] `collector-pod-security.yaml` PSP narrowed
- [x] PR merged to `rc-collector-drop-privs` branch

**Phase 2 (THIS WORK):**
- [ ] `ContainerStartConfig` has `CapAdd` field
- [ ] Docker executor wires `CapAdd` to `HostConfig.CapAdd`
- [ ] CRI executor wires `CapAdd` to `LinuxContainerSecurityContext.Capabilities`
- [ ] K8s collector manager uses capability-based SecurityContext
- [ ] Docker collector manager uses `Privileged: false` + `CapAdd`
- [ ] All existing integration tests pass on at least one RHEL/RHCOS VM
- [ ] All existing integration tests pass on at least one Ubuntu/COS VM
- [ ] K8s integration tests pass on KinD
- [ ] No regression in benchmark tests

**Phase 3 (FUTURE):**
- [ ] `collector.privilegedMode` Helm value with conditional rendering
- [ ] Custom `stackrox-collector` SCC created
- [ ] `openshift.io/required-scc` annotation updated
- [ ] Operator CRD wired (if user-facing control desired)
- [ ] SELinux enforcing validation on OpenShift
- [ ] Documentation updated (support matrix, upgrade guide)