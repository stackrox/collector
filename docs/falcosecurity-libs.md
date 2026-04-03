# falcosecurity-libs

## Overview

The falcosecurity-libs submodule provides kernel instrumentation via CO-RE BPF, enabling syscall tracing and event capture. This is a StackRox fork of upstream Falco libs, customized for network flow tracking and process monitoring.

**Version:** 3.21.6
**Driver:** CO-RE BPF only (kernel >= 5.8 with BTF)
**Location:** `falcosecurity-libs/` submodule

## Architecture Layers

Collector consumes falcosecurity-libs through three layers:

**libsinsp** (`userspace/libsinsp/`)
Event enrichment, filtering, and state tracking. Provides `sinsp` inspector class consumed by `collector/lib/system-inspector/Service.cpp:SystemInspectorService`. Maintains thread table, FD table, container metadata, and filter engine.

**libscap** (`userspace/libscap/`)
Ring buffer management and event parsing. Reads from per-CPU BPF ring buffers, parses `ppm_evt_hdr` structures, extracts syscall parameters. Used via `scap_open()`, `scap_next()` in libsinsp.

**Modern BPF Driver** (`driver/modern_bpf/`)
CO-RE BPF programs attached to tracepoints. Built as skeleton header `bpf_probe.skel.h` embedded in collector binary. Loaded by `collector/lib/KernelDriver.h:ModernBPFDriver`.

## Syscalls Instrumented

Network events consumed by `collector/lib/NetworkSignalHandler.cpp`:
- connect, accept, accept4, bind, listen
- sendto, recvfrom, sendmsg, recvmsg
- socket, socketpair, shutdown, close

Process events consumed by `collector/lib/ProcessSignalFormatter.cpp`:
- execve, execveat (execution)
- clone, clone3, fork, vfork (creation)
- exit (via sched_process_exit tracepoint)

File I/O: open, openat, read, write, close, dup
Memory: mmap, munmap, mprotect, brk
Permissions: setuid, setgid, capset

Modern BPF excludes certain syscalls (configured in `CMakeLists.txt`):
```cmake
MODERN_BPF_EXCLUDE_PROGS "^(openat2|ppoll|setsockopt|io_uring_setup|nanosleep)$"
```

## Event Flow

1. Kernel tracepoints trigger BPF programs (sys_enter/sys_exit, sched_process_exit)
2. BPF programs write `ppm_evt_hdr` structures to per-CPU ring buffers
3. libscap polls ring buffers, parses events via `scap_next()`
4. libsinsp enriches events with container metadata, thread info, FD state
5. sinsp filter engine applies event filters
6. SystemInspectorService dispatches to NetworkSignalHandler or ProcessSignalFormatter
7. Handlers feed ConnTracker or send gRPC signals to Sensor

## Container Metadata

libsinsp resolves container IDs to Kubernetes pod/namespace via container engine APIs:

**Engines** (`userspace/libsinsp/container_engine/`):
- Docker
- containerd (CRI)
- CRI-O
- Podman

**Metadata extracted:**
- Container ID, pod name/namespace
- Image name and digest
- Labels, annotations
- Network namespaces

Used by `collector/lib/ContainerMetadata.cpp` wrapping `sinsp::get_container_manager()`.

## StackRox Configuration

From `collector/CMakeLists.txt`:

```cmake
set(BUILD_LIBSCAP_MODERN_BPF ON)
set(BUILD_DRIVER OFF)  # No kernel module
set(SINSP_SLIM_THREADINFO ON)  # Reduce memory
set(MODERN_BPF_DEBUG_MODE ${BPF_DEBUG_MODE})

add_definitions(-DSCAP_SOCKET_ONLY_FD)
add_definitions("-DINTERESTING_SUBSYS=\"perf_event\", \"cpu\", \"cpuset\", \"memory\"")
set(SCAP_HOST_ROOT_ENV_VAR_NAME "COLLECTOR_HOST_ROOT")
```

Tunables in `collector/lib/CollectorConfig.h`:
- `sinsp_cpu_per_buffer_`: CPUs per ring buffer (default: 0 = 1:1)
- `sinsp_total_buffer_size_`: Total ring buffer size (default: 512 MB)
- `sinsp_thread_cache_size_`: Thread cache size (default: 32768)

## StackRox Fork Differences

ROX-31971 addressed BPF verifier issues with clang > 19. Multiple commits adjusted exec/fork tail call distribution to satisfy verifier complexity limits. ROX-24938 fixed Container-Optimized OS verifier failures. ROX-18856 enabled getsockopt syscall for async connection status tracking.

Kernel compatibility fixes for 6.15, 6.7, RHEL 9.3. PowerPC support added via ppc64le architecture commits. UDP connectionless syscall tracking improved for network observability.

**Fork repository:** github.com/stackrox/falcosecurity-libs (branch: module-version-2.10)
**Upstream:** github.com/falcosecurity/libs

Merge strategy: StackRox maintains versioned branches (0.17.3-stackrox, 0.18.1-stackrox, 0.21.0-stackrox, 0.23.1-stackrox), periodically rebases on upstream releases, applies custom patches. Update workflow requires testing across supported kernel versions, verifying BPF verifier compatibility, checking API/schema compatibility. See [falco-update.md](falco-update.md) for the rebase process.

## Collector Integration

**SystemInspectorService** (`collector/lib/system-inspector/Service.cpp`)
Creates sinsp instance, opens modern BPF engine via `inspector->open_modern_bpf()`, registers signal handlers, runs event loop with `inspector->next()`.

**NetworkSignalHandler** (`collector/lib/NetworkSignalHandler.cpp`)
Consumes network events, extracts connection tuples (src IP:port → dst IP:port), feeds ConnTracker for aggregation. Uses `system_inspector::EventExtractor` wrapper around libsinsp APIs.

**ProcessSignalHandler** (`collector/lib/ProcessSignalFormatter.cpp`)
Consumes process events, builds lineage (parent → child), extracts cmdline/env, sends to Sensor via gRPC.

**ContainerMetadata** (`collector/lib/ContainerMetadata.cpp`)
Wraps `sinsp::get_container_manager()`, resolves container IDs to K8s pod/namespace.

**KernelDriver** (`collector/lib/KernelDriver.h`)
ModernBPFDriver class manages probe lifecycle, references `g_syscall_table` from libscap, handles loading/teardown.

## Debugging

Enable BPF debug mode:
```cmake
set(MODERN_BPF_DEBUG_MODE ON)
```
Logs to `/sys/kernel/debug/tracing/trace_pipe` (requires CONFIG_DEBUG_FS).

Capture file replay: libscap records events to `.scap` files for offline reproduction.

Verifier failures: check kernel version, review `/var/log/kern.log`, adjust tail call distribution or program complexity, may require clang/LLVM version changes.

## Performance

Ring buffer sizing: default 512 MB total. Too small causes event drops, too large causes memory pressure. Per-CPU buffers sized via `sinsp_cpu_per_buffer_`.

Event filtering: `SCAP_SOCKET_ONLY_FD` processes socket FDs only, reducing userspace load. Limited cgroup subsystems tracked.

Slim threadinfo: `SINSP_SLIM_THREADINFO` reduces memory ~60% by omitting env vars and full FD snapshots. Trade-off: less detail in process signals.

## Security

Kernel instrumentation requires CAP_SYS_ADMIN or CAP_BPF/CAP_PERFMON (kernel >= 5.8). Syscall arguments may contain sensitive data. BPF program loading restricted to collector pod. Ring buffer access requires privileges.

## History

ROX-7482 (2022-01-31) migrated from deprecated sysdig to falcosecurity-libs, enabling CO-RE modern BPF. Version updates through 2022-2023 (0.21.0, 0.23.1, 2.11, 0.18.1). Kernel compatibility fixes for Linux 6.7, 9.4, RHEL 9.3, 6.11.4. Clang/verifier improvements 2024-2026 for modern toolchains.

ROX-32740 explores migrating network flow collection to OpenShift Network Observability operator, potentially deprecating networking-specific Falco probes.
