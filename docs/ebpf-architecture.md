# eBPF Architecture

This document explains how collector's CO-RE BPF driver works at the kernel level, including tracepoint architecture, tail call dispatch, syscall coverage, event format, and the complete path from kernel to userspace.

**Audience:** Engineers adding syscall monitoring, debugging verifier failures, or understanding performance characteristics.

**Prerequisites:** Basic BPF knowledge, familiarity with [falcosecurity-libs.md](./falcosecurity-libs.md) and [lib/system.md](./lib/system.md).

## Table of Contents

1. [CO-RE BPF Overview](#co-re-bpf-overview)
2. [Tracepoint Architecture](#tracepoint-architecture)
3. [Tail Call Dispatch](#tail-call-dispatch)
4. [Syscall Coverage](#syscall-coverage)
5. [Event Format and Ring Buffers](#event-format-and-ring-buffers)
6. [Event Flow: Kernel to Userspace](#event-flow-kernel-to-userspace)
7. [Adding New Syscall Monitoring](#adding-new-syscall-monitoring)
8. [Verifier Constraints and Pitfalls](#verifier-constraints-and-pitfalls)

---

## CO-RE BPF Overview

Collector uses **CO-RE (Compile Once, Run Everywhere) BPF**, the modern approach to kernel instrumentation. Unlike classic eBPF or kernel modules, CO-RE programs:

- **Compile once**: BPF bytecode embedded in collector binary (`bpf_probe.skel.h` skeleton)
- **Run everywhere**: libbpf relocates struct offsets at load time using BTF (BPF Type Format)
- **No kernel headers**: Uses `vmlinux.h` generated from kernel's BTF at runtime
- **Ring buffers**: Efficient per-CPU ring buffers (`BPF_MAP_TYPE_RINGBUF`) instead of perf buffers
- **Tracing programs**: `BPF_PROG_TYPE_TRACING` attached to `tp_btf` (BTF-enabled tracepoints)

**Requirements:**
- Kernel >= 5.8 with BTF support (`CONFIG_DEBUG_INFO_BTF=y`)
- BTF available at `/sys/kernel/btf/vmlinux` or `/boot/vmlinux-*`
- CAP_BPF + CAP_PERFMON (or CAP_SYS_ADMIN on older kernels)

**Location:** `falcosecurity-libs/driver/modern_bpf/`

**Build output:** Compiled to `bpf_probe.skel.h`, embedded in collector, loaded by `ModernBPFDriver::Setup()` in `collector/lib/KernelDriver.h`.

---

## Tracepoint Architecture

### Attached Programs

The modern BPF driver attaches programs to kernel tracepoints and schedules events. These are the **entry points** from the kernel into BPF:

**Syscall Dispatchers** (attach to all syscalls):
- `sys_enter` → `syscall_enter.bpf.c` → dispatches to per-syscall enter handlers
- `sys_exit` → `syscall_exit.bpf.c` → dispatches to per-syscall exit handlers

**Scheduler Tracepoints** (process lifecycle):
- `sched_process_exit` → `sched_process_exit.bpf.c` → process termination (PPME_PROCEXIT_1_E)
- `sched_process_fork` → `sched_process_fork.bpf.c` → fork/clone tracking
- `sched_process_exec` → `sched_process_exec.bpf.c` → execve success path
- `sched_switch` → `sched_switch.bpf.c` → context switch (improves process tracking)

**Other Tracepoints**:
- `signal_deliver` → `signal_deliver.bpf.c` → signal delivery
- `page_fault_user` / `page_fault_kernel` → page fault tracking

**Attachment mechanism:**

```c
SEC("tp_btf/sys_enter")
int BPF_PROG(sys_enter, struct pt_regs* regs, long syscall_id) {
    // Dispatcher logic
    bpf_tail_call(ctx, &syscall_enter_tail_table, syscall_id);
    return 0;
}
```

`SEC("tp_btf/...")` tells libbpf to attach to a BTF-enabled tracepoint. `BPF_PROG()` macro provides type-safe arguments from the tracepoint's signature.

**Source:** `falcosecurity-libs/driver/modern_bpf/programs/attached/`

---

## Tail Call Dispatch

### Why Tail Calls?

BPF verifier limits programs to 1 million instructions. A single dispatcher handling 158 syscalls would exceed this. **Tail calls** (`bpf_tail_call()`) replace the current program with another, resetting the instruction count.

### Tail Call Tables

Three `BPF_MAP_TYPE_PROG_ARRAY` maps route execution:

**1. syscall_enter_tail_table**
- Indexed by syscall ID (e.g., `__NR_connect = 42`)
- Maps syscall → enter event handler
- Populated by libscap during BPF skeleton load
- Example: `syscall_enter_tail_table[42] = connect_e`

**2. syscall_exit_tail_table**
- Indexed by syscall ID
- Maps syscall → exit event handler
- Example: `syscall_exit_tail_table[42] = connect_x`

**3. extra_syscall_calls**
- Indexed by predefined codes (T1_EXECVE_X, T2_EXECVE_X, T1_DROP_E, etc.)
- Used for multi-stage events (execve needs 3 programs due to verifier limits)
- Used for special events (hotplug, drop notifications)

**Dispatch flow (sys_enter example):**

```c
SEC("tp_btf/sys_enter")
int BPF_PROG(sys_enter, struct pt_regs* regs, long syscall_id) {
    // 1. Filter: check if syscall is interesting
    if (!syscalls_dispatcher__64bit_interesting_syscall(syscall_id)) {
        return 0;  // Drop uninteresting syscalls
    }

    // 2. Sampling: drop events if in sampling mode
    if (sampling_logic_enter(ctx, syscall_id)) {
        return 0;
    }

    // 3. Tail call to specific handler
    bpf_tail_call(ctx, &syscall_enter_tail_table, syscall_id);
    return 0;  // Fallback if tail call fails
}
```

**Why this works:**
- Dispatcher: ~50 instructions (well within limits)
- Each syscall handler: separate program, separate instruction budget
- No shared stack between programs (tail call is a **replace**, not a call)

**Limitations:**
- Max 33 tail calls deep (kernel limitation)
- No return from tail call (one-way jump)
- Complex events (execve) need multiple stages: `execve_x` → `t1_execve_x` → `t2_execve_x`

**Source:** `falcosecurity-libs/driver/modern_bpf/maps/maps.h` (tail table definitions)

---

## Syscall Coverage

The modern BPF driver implements **158 syscall handlers** covering network, process, file I/O, permissions, and IPC.

### Network Syscalls (consumed by NetworkSignalHandler)

| Syscall | Enter Event | Exit Event | Data Captured | Collector Handler |
|---------|-------------|------------|---------------|-------------------|
| `connect` | PPME_SOCKET_CONNECT_E | PPME_SOCKET_CONNECT_X | fd, sockaddr → socktuple (src/dst IP:port) | `NetworkSignalHandler::GetConnection()` |
| `accept` | PPME_SOCKET_ACCEPT_E | PPME_SOCKET_ACCEPT_X | listen_fd → new_fd, socktuple | `NetworkSignalHandler::GetConnection()` |
| `accept4` | PPME_SOCKET_ACCEPT4_E | PPME_SOCKET_ACCEPT4_X | listen_fd, flags → new_fd, socktuple | `NetworkSignalHandler::GetConnection()` |
| `bind` | PPME_SOCKET_BIND_E | PPME_SOCKET_BIND_X | fd, sockaddr → bind address | Connection tracking (server role) |
| `listen` | PPME_SOCKET_LISTEN_E | PPME_SOCKET_LISTEN_X | fd, backlog | Connection tracking (server role) |
| `close` | PPME_SOCKET_CLOSE_E | PPME_SOCKET_CLOSE_X | fd → connection closure | `ConnTracker::UpdateConnection()` |
| `shutdown` | PPME_SOCKET_SHUTDOWN_E | PPME_SOCKET_SHUTDOWN_X | fd, how (SHUT_RD/WR/RDWR) | Connection tracking |
| `sendto` | PPME_SOCKET_SENDTO_E | PPME_SOCKET_SENDTO_X | fd, size, dest_addr → bytes_sent | Byte tracking (if enabled) |
| `recvfrom` | PPME_SOCKET_RECVFROM_E | PPME_SOCKET_RECVFROM_X | fd, size → bytes_recv, src_addr | Byte tracking (if enabled) |
| `sendmsg` | PPME_SOCKET_SENDMSG_E | PPME_SOCKET_SENDMSG_X | fd, msghdr → bytes_sent | Byte tracking (if enabled) |
| `recvmsg` | PPME_SOCKET_RECVMSG_E | PPME_SOCKET_RECVMSG_X | fd, msghdr → bytes_recv | Byte tracking (if enabled) |
| `sendmmsg` | PPME_SOCKET_SENDMMSG_E | PPME_SOCKET_SENDMMSG_X | fd, vlen → messages_sent | Batch message tracking |
| `recvmmsg` | PPME_SOCKET_RECVMMSG_E | PPME_SOCKET_RECVMMSG_X | fd, vlen → messages_recv | Batch message tracking |
| `socket` | PPME_SOCKET_SOCKET_E | PPME_SOCKET_SOCKET_X | domain, type, protocol → fd | Socket creation |
| `socketpair` | PPME_SOCKET_SOCKETPAIR_E | PPME_SOCKET_SOCKETPAIR_X | domain, type, protocol → fd[2] | Unix socket pair |
| `getsockopt` | PPME_SOCKET_GETSOCKOPT_E | PPME_SOCKET_GETSOCKOPT_X | fd, level, optname → optval | Async connect status (ROX-18856) |
| `setsockopt` | PPME_SOCKET_SETSOCKOPT_E | PPME_SOCKET_SETSOCKOPT_X | fd, level, optname, optval | Socket configuration |
| `getsockname` | PPME_SOCKET_GETSOCKNAME_E | PPME_SOCKET_GETSOCKNAME_X | fd → local sockaddr | Local address lookup |
| `getpeername` | PPME_SOCKET_GETPEERNAME_E | PPME_SOCKET_GETPEERNAME_X | fd → remote sockaddr | Peer address lookup |

**Example: connect syscall**

```c
// falcosecurity-libs/driver/modern_bpf/programs/tail_called/events/syscall_dispatched_events/connect.bpf.c

SEC("tp_btf/sys_enter")
int BPF_PROG(connect_e, struct pt_regs *regs, long id) {
    struct auxiliary_map *auxmap = auxmap__get();
    auxmap__preload_event_header(auxmap, PPME_SOCKET_CONNECT_E);

    unsigned long args[3] = {0};
    extract__network_args(args, 3, regs);  // fd, sockaddr*, addrlen

    int32_t socket_fd = (int32_t)args[0];
    auxmap__store_s64_param(auxmap, (int64_t)socket_fd);

    unsigned long sockaddr_ptr = args[1];
    uint16_t addrlen = (uint16_t)args[2];
    auxmap__store_sockaddr_param(auxmap, sockaddr_ptr, addrlen);

    auxmap__finalize_event_header(auxmap);
    auxmap__submit_event(auxmap);  // Push to ring buffer
    return 0;
}

SEC("tp_btf/sys_exit")
int BPF_PROG(connect_x, struct pt_regs *regs, long ret) {
    struct auxiliary_map *auxmap = auxmap__get();
    auxmap__preload_event_header(auxmap, PPME_SOCKET_CONNECT_X);

    unsigned long socket_fd = 0;
    extract__network_args(&socket_fd, 1, regs);

    // Return code (0 = success, -EINPROGRESS = async, <0 = error)
    auxmap__store_s64_param(auxmap, ret);

    // Extract socktuple (src IP:port → dst IP:port) from kernel socket struct
    if (ret == 0 || ret == -EINPROGRESS) {
        auxmap__store_socktuple_param(auxmap, (int32_t)socket_fd, OUTBOUND, NULL);
    } else {
        auxmap__store_empty_param(auxmap);
    }

    auxmap__store_s64_param(auxmap, (int64_t)(int32_t)socket_fd);

    auxmap__finalize_event_header(auxmap);
    auxmap__submit_event(auxmap);
    return 0;
}
```

**Userspace consumption:**

`NetworkSignalHandler::HandleSignal()` receives `PPME_SOCKET_CONNECT_X`, extracts socktuple via `EventExtractor`, creates `Connection` object, feeds `ConnTracker`.

### Process Syscalls (consumed by ProcessSignalHandler)

| Syscall | Enter Event | Exit Event | Data Captured | Collector Handler |
|---------|-------------|------------|---------------|-------------------|
| `execve` | PPME_SYSCALL_EXECVE_19_E | PPME_SYSCALL_EXECVE_19_X | filename → pid, tid, exe, args, env, cwd, cgroups, caps, ... (27 params) | `ProcessSignalFormatter` → gRPC to Sensor |
| `execveat` | PPME_SYSCALL_EXECVEAT_E | PPME_SYSCALL_EXECVEAT_X | dirfd, pathname, flags → (same as execve) | `ProcessSignalFormatter` |
| `clone` | PPME_SYSCALL_CLONE_E | PPME_SYSCALL_CLONE_X | flags, stack, ptid, ctid → child_tid, clone_flags | Process lineage tracking |
| `clone3` | PPME_SYSCALL_CLONE3_E | PPME_SYSCALL_CLONE3_X | clone_args → child_tid, clone_flags | Process lineage tracking |
| `fork` | PPME_SYSCALL_FORK_E | PPME_SYSCALL_FORK_X | None → child_pid | Process lineage tracking |
| `vfork` | PPME_SYSCALL_VFORK_E | PPME_SYSCALL_VFORK_X | None → child_pid | Process lineage tracking |

**Special: sched_process_exit tracepoint**

```c
// falcosecurity-libs/driver/modern_bpf/programs/attached/events/sched_process_exit.bpf.c

SEC("tp_btf/sched_process_exit")
int BPF_PROG(sched_proc_exit, struct task_struct *task) {
    struct auxiliary_map *auxmap = auxmap__get();
    auxmap__preload_event_header(auxmap, PPME_PROCEXIT_1_E);

    // Extract exit status from task_struct
    int32_t exit_code = 0;
    READ_TASK_FIELD_INTO(&exit_code, task, exit_code);
    auxmap__store_s64_param(auxmap, (int64_t)exit_code);

    // Extract return code
    int32_t ret = __WEXITSTATUS(exit_code);
    auxmap__store_s64_param(auxmap, (int64_t)ret);

    // Extract termination signal (if any)
    uint8_t sig = 0;
    if (__WIFSIGNALED(exit_code) != 0) {
        sig = __WTERMSIG(exit_code);
    }
    auxmap__store_u8_param(auxmap, sig);

    // Core dump flag
    uint8_t core = __WCOREDUMP(exit_code) != 0;
    auxmap__store_u8_param(auxmap, core);

    // Find reaper for orphaned children
    int32_t reaper_pid = 0;
    struct list_head *head = &(task->children);
    struct list_head *next_child = BPF_CORE_READ(head, next);
    if (next_child != head) {
        reaper_pid = find_new_reaper_pid(task);  // Complex logic, see below
    }
    auxmap__store_s64_param(auxmap, (int64_t)reaper_pid);

    auxmap__finalize_event_header(auxmap);
    auxmap__submit_event(auxmap);
    return 0;
}
```

**Reaper logic:** When a process exits, its children are reparented to:
1. Another thread in the same thread group (if alive)
2. A sub-reaper (process with `prctl(PR_SET_CHILD_SUBREAPER)`)
3. PID 1 (init) in the current PID namespace

This is implemented with `#pragma unroll` loops to satisfy verifier complexity limits (see ROX-31971).

### File I/O Syscalls

| Syscall | Data Captured | Use Case |
|---------|---------------|----------|
| `open`, `openat`, `openat2` | path, flags, mode → fd | File access tracking |
| `read`, `readv`, `pread64`, `preadv` | fd, size → bytes_read | Data flow analysis |
| `write`, `writev`, `pwrite64`, `pwritev` | fd, size → bytes_written | Data flow analysis |
| `close` | fd → (fd closure) | Resource cleanup tracking |
| `dup`, `dup2`, `dup3` | oldfd → newfd | FD aliasing |

### Permission/Security Syscalls

| Syscall | Data Captured | Use Case |
|---------|---------------|----------|
| `setuid`, `setgid`, `setreuid`, `setregid`, `setresuid`, `setresgid` | uid/gid changes | Privilege escalation detection |
| `capset` | capability changes | Capability tracking |
| `prctl` | operation, args | Process behavior modification |
| `seccomp` | mode, filter | Sandboxing detection |
| `ptrace` | request, pid | Debugging/injection detection |

### Full Syscall List

158 syscalls instrumented. See complete list:
```bash
ls falcosecurity-libs/driver/modern_bpf/programs/tail_called/events/syscall_dispatched_events/
```

**Excluded syscalls** (configured in `CMakeLists.txt`):
```cmake
MODERN_BPF_EXCLUDE_PROGS "^(openat2|ppoll|setsockopt|io_uring_setup|nanosleep)$"
```
These syscalls have handlers but may be disabled to reduce overhead.

---

## Event Format and Ring Buffers

### Event Header: ppm_evt_hdr

Every event starts with a fixed header:

```c
// falcosecurity-libs/driver/ppm_events_public.h

struct ppm_evt_hdr {
    uint64_t ts;      // Timestamp (nanoseconds since boot, from bpf_ktime_get_boot_ns())
    uint64_t tid;     // Thread ID that triggered the event
    uint32_t len;     // Total event length including header and parameters
    uint16_t type;    // ppm_event_code (e.g., PPME_SOCKET_CONNECT_X)
    uint32_t nparams; // Number of parameters following header
} __attribute__((packed));
```

### Event Parameters

After the header, parameters are serialized as type-length-value (TLV):

**Parameter types** (from `ppm_param_type`):
- `PT_FD`: file descriptor (int32)
- `PT_ERRNO`: error code / return value (int64)
- `PT_SOCKADDR`: socket address (family + IP + port)
- `PT_SOCKTUPLE`: full connection tuple (src IP:port → dst IP:port, protocol)
- `PT_CHARBUF`: null-terminated string
- `PT_CHARBUFARRAY`: array of strings
- `PT_PID`: process/thread ID
- `PT_UID`, `PT_GID`: user/group IDs
- `PT_FLAGS32`: bitmask flags

**Variable-size encoding:**

```c
struct auxiliary_map {
    uint8_t data[AUXILIARY_MAP_SIZE];  // Raw event bytes (max 128KB)
    uint64_t payload_pos;              // Current write position
    uint8_t lengths_pos;               // Parameter count
    uint16_t event_type;               // PPME_* code
};
```

Each BPF program uses a **per-CPU auxiliary map** to build the event:

```c
// 1. Initialize header
auxmap__preload_event_header(auxmap, PPME_SOCKET_CONNECT_X);

// 2. Store parameters
auxmap__store_s64_param(auxmap, return_code);               // PT_ERRNO
auxmap__store_socktuple_param(auxmap, fd, OUTBOUND, NULL);  // PT_SOCKTUPLE
auxmap__store_s64_param(auxmap, fd);                        // PT_FD

// 3. Finalize header (sets len, nparams, ts, tid)
auxmap__finalize_event_header(auxmap);

// 4. Submit to ring buffer
auxmap__submit_event(auxmap);
```

**Why auxiliary maps instead of direct ring buffer writes?**

Ring buffers require reserving space upfront, but syscall events are variable-size (e.g., execve args can be 64KB). The verifier can't prove bounds for direct writes. Auxiliary maps work around this:
- Per-CPU map (no contention)
- Fixed size (128KB, verifier-friendly)
- Single `bpf_ringbuf_output()` call at the end

### Ring Buffer Architecture

**Per-CPU ring buffers** (`BPF_MAP_TYPE_RINGBUF`):

```c
// falcosecurity-libs/driver/modern_bpf/maps/maps.h (simplified)

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 512 * 1024 * 1024);  // 512MB total, divided across CPUs
} ringbuf __weak SEC(".maps");
```

**Configuration:**
- Total size: `sinsp_total_buffer_size_` (default 512MB)
- CPUs per buffer: `sinsp_cpu_per_buffer_` (default 0 = 1:1)
- Example: 128 CPUs, 512MB total, 1:1 → 128 buffers of 4MB each

**Why per-CPU?**
- No locking (each CPU writes to its own buffer)
- Cache-friendly (data stays local)
- Scalable to 100+ CPUs

**Overflow handling:**
- If buffer full, `bpf_ringbuf_output()` fails silently
- Userspace detects drops via ring buffer headers
- `nDropsBuffer` stat incremented

**Userspace polling:**
- libscap uses `epoll()` on ring buffer FDs
- `scap_next()` reads from next available buffer in round-robin

---

## Event Flow: Kernel to Userspace

Complete path from syscall to collector handler:

### 1. Kernel Tracepoint Fires

```
Application calls connect(fd, &addr, addrlen)
    ↓
Kernel enters sys_connect()
    ↓
Tracepoint: trace_sys_enter(regs, __NR_connect)
    ↓
BPF program attached to tp_btf/sys_enter triggers
```

### 2. Dispatcher Filters and Tail Calls

```c
// falcosecurity-libs/driver/modern_bpf/programs/attached/dispatchers/syscall_enter.bpf.c

int BPF_PROG(sys_enter, struct pt_regs* regs, long syscall_id) {
    // syscall_id = 42 (__NR_connect)

    // Check if interesting
    if (!g_64bit_interesting_syscalls_table[42]) return 0;

    // Check sampling
    if (sampling_logic_enter(ctx, 42)) return 0;

    // Tail call to connect_e
    bpf_tail_call(ctx, &syscall_enter_tail_table, 42);
}
```

### 3. Syscall Handler Builds Event

```c
// connect.bpf.c:connect_e (enter event)

int BPF_PROG(connect_e, struct pt_regs *regs, long id) {
    struct auxiliary_map *auxmap = maps__get_auxiliary_map(cpu_id);

    auxmap->event_type = PPME_SOCKET_CONNECT_E;
    auxmap->payload_pos = sizeof(struct ppm_evt_hdr);

    // Extract syscall args from pt_regs
    int fd = (int)regs->rdi;           // arg 0
    struct sockaddr *addr = regs->rsi; // arg 1
    socklen_t addrlen = regs->rdx;     // arg 2

    // Store parameters
    auxmap__store_s64_param(auxmap, fd);
    auxmap__store_sockaddr_param(auxmap, addr, addrlen);

    // Fill header: ts, tid, len, nparams
    auxmap__finalize_event_header(auxmap);

    // Submit to ring buffer
    bpf_ringbuf_output(&ringbuf, auxmap->data, auxmap->payload_pos, 0);
}
```

### 4. Kernel Executes Syscall

```
connect_e submitted
    ↓
BPF program returns
    ↓
Kernel executes actual sys_connect() logic
    ↓
Kernel returns (success or error)
    ↓
Tracepoint: trace_sys_exit(regs, ret)
    ↓
BPF program attached to tp_btf/sys_exit triggers
```

### 5. Exit Handler Captures Result

```c
// connect.bpf.c:connect_x (exit event)

int BPF_PROG(connect_x, struct pt_regs *regs, long ret) {
    // ret = 0 (success) or -EINPROGRESS or -errno

    struct auxiliary_map *auxmap = auxmap__get();
    auxmap__preload_event_header(auxmap, PPME_SOCKET_CONNECT_X);

    // Store return code
    auxmap__store_s64_param(auxmap, ret);

    // On success/EINPROGRESS, extract connection tuple from kernel socket
    if (ret == 0 || ret == -EINPROGRESS) {
        struct socket *sock = sockfd_lookup(fd);
        struct sock *sk = sock->sk;

        // Extract: src IP, src port, dst IP, dst port, protocol
        uint32_t saddr = sk->__sk_common.skc_rcv_saddr;
        uint16_t sport = sk->__sk_common.skc_num;
        uint32_t daddr = sk->__sk_common.skc_daddr;
        uint16_t dport = ntohs(sk->__sk_common.skc_dport);

        // Store as socktuple
        auxmap__store_socktuple_param(auxmap, fd, OUTBOUND, NULL);
    } else {
        auxmap__store_empty_param(auxmap);
    }

    auxmap__store_s64_param(auxmap, fd);
    auxmap__finalize_event_header(auxmap);
    auxmap__submit_event(auxmap);
}
```

### 6. Userspace Polls Ring Buffer

```cpp
// falcosecurity-libs/userspace/libscap/scap.c (simplified)

int32_t scap_next(scap_t* handle, scap_evt** pevent, uint16_t* pcpuid) {
    // Poll ring buffers via epoll
    int n = epoll_wait(handle->m_epollfd, events, handle->m_ndevs, timeout);

    for (int i = 0; i < n; i++) {
        int cpu = events[i].data.fd;
        struct ringbuf_map* rb = &handle->m_devs[cpu];

        // Read event from ring buffer
        struct ppm_evt_hdr* hdr = ringbuf_read(rb);

        *pevent = (scap_evt*)hdr;
        *pcpuid = cpu;
        return SCAP_SUCCESS;
    }
}
```

### 7. libsinsp Enriches Event

```cpp
// falcosecurity-libs/userspace/libsinsp/sinsp.cpp (simplified)

int32_t sinsp::next(sinsp_evt** evt) {
    scap_evt* scap_evt;
    uint16_t cpuid;

    // Get raw event from libscap
    int32_t res = scap_next(m_h, &scap_evt, &cpuid);

    // Wrap in sinsp_evt (adds thread/FD context)
    m_evt.set_scap_evt(scap_evt);

    // Lookup thread info from cache
    threadinfo* tinfo = m_thread_manager->find_thread(scap_evt->tid);
    m_evt.set_threadinfo(tinfo);

    // For socket events, lookup FD info
    if (scap_evt->type == PPME_SOCKET_CONNECT_X) {
        sinsp_fdinfo* fdinfo = tinfo->get_fd(fd);
        m_evt.set_fdinfo(fdinfo);
    }

    // Resolve container metadata (if available)
    if (tinfo->m_container_id != "") {
        container_info* cinfo = m_container_manager->get_container(tinfo->m_container_id);
        m_evt.set_container_info(cinfo);
    }

    *evt = &m_evt;
    return SCAP_SUCCESS;
}
```

### 8. SystemInspector Dispatches to Handlers

```cpp
// collector/lib/system-inspector/Service.cpp (simplified)

void SystemInspectorService::Run() {
    while (running_) {
        sinsp_evt* evt;
        int32_t res = inspector_->next(&evt);

        if (res == SCAP_SUCCESS) {
            stats_.nEvents++;

            // Dispatch to registered handlers
            for (auto& handler : signal_handlers_) {
                if (handler->IsInterested(evt->get_type())) {
                    handler->HandleSignal(evt);
                }
            }
        }
    }
}
```

### 9. NetworkSignalHandler Processes Event

```cpp
// collector/lib/NetworkSignalHandler.cpp (simplified)

SignalHandler::Result NetworkSignalHandler::HandleSignal(sinsp_evt* evt) {
    if (evt->get_type() != PPME_SOCKET_CONNECT_X) {
        return SignalHandler::IGNORED;
    }

    // Extract connection tuple
    std::optional<Connection> conn = GetConnection(evt);
    if (!conn) return SignalHandler::IGNORED;

    // Feed to connection tracker
    conn_tracker_->UpdateConnection(*conn);

    return SignalHandler::SUCCESS;
}

std::optional<Connection> NetworkSignalHandler::GetConnection(sinsp_evt* evt) {
    // Use EventExtractor to safely access event parameters
    auto tuple = event_extractor_->get_socktuple(evt);
    auto container_id = event_extractor_->get_container_id(evt);
    auto pid = event_extractor_->get_tid(evt);

    return Connection{
        .tuple = tuple,
        .container_id = container_id,
        .pid = pid,
        .timestamp = evt->get_ts()
    };
}
```

### 10. ConnTracker Aggregates

```cpp
// collector/lib/ConnTracker.cpp (simplified)

void ConnTracker::UpdateConnection(const Connection& conn) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto key = MakeKey(conn.tuple);
    auto& agg = connections_[key];

    agg.bytes_sent += conn.bytes_sent;
    agg.bytes_recv += conn.bytes_recv;
    agg.last_seen = conn.timestamp;

    // Periodically flush to Sensor via gRPC
    if (ShouldFlush(agg)) {
        SendToSensor(agg);
    }
}
```

**Full flow diagram:**

```
Application syscall (connect)
    ↓
tp_btf/sys_enter tracepoint → sys_enter.bpf.c dispatcher
    ↓
bpf_tail_call → connect_e handler (PPME_SOCKET_CONNECT_E)
    ↓
Event → auxiliary_map → ring buffer
    ↓
Kernel executes sys_connect()
    ↓
tp_btf/sys_exit tracepoint → sys_exit.bpf.c dispatcher
    ↓
bpf_tail_call → connect_x handler (PPME_SOCKET_CONNECT_X)
    ↓
Event → auxiliary_map → ring buffer
    ↓
libscap: scap_next() polls ring buffer → scap_evt
    ↓
libsinsp: sinsp::next() enriches → threadinfo, fdinfo, container_info
    ↓
SystemInspectorService: dispatches to signal handlers
    ↓
NetworkSignalHandler: extracts Connection
    ↓
ConnTracker: aggregates, sends to Sensor via gRPC
```

---

## Adding New Syscall Monitoring

Step-by-step guide to add monitoring for a new syscall (example: `openat`).

### 1. Verify Driver Support

Check if falcosecurity-libs already instruments the syscall:

```bash
ls falcosecurity-libs/driver/modern_bpf/programs/tail_called/events/syscall_dispatched_events/ | grep openat
```

If `openat.bpf.c` exists, the driver already captures it. Skip to step 5 (enable in collector config).

If missing, you'll need to add it to falcosecurity-libs (upstream contribution or StackRox fork).

### 2. Add BPF Handler (in falcosecurity-libs)

Create `falcosecurity-libs/driver/modern_bpf/programs/tail_called/events/syscall_dispatched_events/openat.bpf.c`:

```c
// SPDX-License-Identifier: GPL-2.0-only OR MIT
#include <helpers/interfaces/variable_size_event.h>

/*=============================== ENTER EVENT ===========================*/

SEC("tp_btf/sys_enter")
int BPF_PROG(openat_e, struct pt_regs *regs, long id) {
    struct auxiliary_map *auxmap = auxmap__get();
    if (!auxmap) return 0;

    auxmap__preload_event_header(auxmap, PPME_SYSCALL_OPENAT_E);

    // Extract syscall arguments
    // openat(int dirfd, const char *pathname, int flags, mode_t mode)
    int32_t dirfd = (int32_t)extract__syscall_argument(regs, 0);
    unsigned long pathname_ptr = extract__syscall_argument(regs, 1);
    int32_t flags = (int32_t)extract__syscall_argument(regs, 2);
    uint32_t mode = (uint32_t)extract__syscall_argument(regs, 3);

    // Store parameters
    auxmap__store_s64_param(auxmap, dirfd);
    auxmap__store_charbuf_param(auxmap, pathname_ptr, MAX_PATH, USER);
    auxmap__store_u32_param(auxmap, flags);
    auxmap__store_u32_param(auxmap, mode);

    auxmap__finalize_event_header(auxmap);
    auxmap__submit_event(auxmap);
    return 0;
}

/*=============================== EXIT EVENT ===========================*/

SEC("tp_btf/sys_exit")
int BPF_PROG(openat_x, struct pt_regs *regs, long ret) {
    struct auxiliary_map *auxmap = auxmap__get();
    if (!auxmap) return 0;

    auxmap__preload_event_header(auxmap, PPME_SYSCALL_OPENAT_X);

    // ret = fd (>= 0) or -errno
    auxmap__store_s64_param(auxmap, ret);

    // Re-extract arguments (not preserved across syscall boundary)
    int32_t dirfd = (int32_t)extract__syscall_argument(regs, 0);
    unsigned long pathname_ptr = extract__syscall_argument(regs, 1);
    int32_t flags = (int32_t)extract__syscall_argument(regs, 2);
    uint32_t mode = (uint32_t)extract__syscall_argument(regs, 3);

    auxmap__store_s64_param(auxmap, dirfd);
    auxmap__store_charbuf_param(auxmap, pathname_ptr, MAX_PATH, USER);
    auxmap__store_u32_param(auxmap, flags);
    auxmap__store_u32_param(auxmap, mode);

    auxmap__finalize_event_header(auxmap);
    auxmap__submit_event(auxmap);
    return 0;
}
```

### 3. Define Event Codes (in falcosecurity-libs)

Add to `falcosecurity-libs/driver/event_table.c`:

```c
[PPME_SYSCALL_OPENAT_E] = {"openat", EC_SYSCALL | EC_FILE, EF_NONE, 4, {
    {"dirfd", PT_FD, PF_DEC},
    {"pathname", PT_FSPATH, PF_NA},
    {"flags", PT_FLAGS32, PF_HEX},
    {"mode", PT_UINT32, PF_OCT}
}},
[PPME_SYSCALL_OPENAT_X] = {"openat", EC_SYSCALL | EC_FILE, EF_NONE, 5, {
    {"res", PT_FD, PF_DEC},
    {"dirfd", PT_FD, PF_DEC},
    {"pathname", PT_FSPATH, PF_NA},
    {"flags", PT_FLAGS32, PF_HEX},
    {"mode", PT_UINT32, PF_OCT}
}},
```

Update `falcosecurity-libs/driver/ppm_events_public.h`:

```c
enum ppm_event_code {
    // ... existing events ...
    PPME_SYSCALL_OPENAT_E = 300,
    PPME_SYSCALL_OPENAT_X = 301,
    // ...
};
```

### 4. Register Tail Calls (in falcosecurity-libs)

Tail calls are auto-registered by libbpf based on section names. No manual registration needed if you use `SEC("tp_btf/sys_enter")` / `SEC("tp_btf/sys_exit")`.

### 5. Enable in Collector Config

Add to `collector/container/config/collection-method.yaml`:

```yaml
syscalls:
  - connect
  - accept
  - close
  - execve
  - openat    # <-- Add here
```

Or configure via environment variable:
```bash
COLLECTION_METHOD="syscalls=connect,accept,close,execve,openat"
```

### 6. Add Collector Handler (if needed)

If you need custom processing (beyond generic event capture):

**Create handler:**

```cpp
// collector/lib/FileAccessHandler.h

class FileAccessHandler : public SignalHandler {
public:
    std::string GetName() override { return "FileAccessHandler"; }

    Result HandleSignal(sinsp_evt* evt) override {
        if (evt->get_type() == PPME_SYSCALL_OPENAT_X) {
            return HandleOpenat(evt);
        }
        return IGNORED;
    }

    std::vector<std::string> GetRelevantEvents() override {
        return {"openat"};
    }

private:
    Result HandleOpenat(sinsp_evt* evt) {
        int64_t ret = evt->get_param(0)->as<int64_t>();  // return code
        if (ret < 0) return IGNORED;  // failed open

        int fd = ret;
        std::string path = evt->get_param(2)->as<std::string>();  // pathname
        uint32_t flags = evt->get_param(3)->as<uint32_t>();

        // Process file access (e.g., track writes to sensitive paths)
        if (flags & O_WRONLY || flags & O_RDWR) {
            LogFileWrite(path, fd);
        }

        return SUCCESS;
    }
};
```

**Register in SystemInspector:**

```cpp
// collector/lib/CollectorService.cpp

void CollectorService::InitSystemInspector() {
    // ... existing handlers ...

    auto file_handler = std::make_unique<FileAccessHandler>();
    system_inspector_->AddSignalHandler(std::move(file_handler));
}
```

### 7. Test

**Build collector:**
```bash
make image
```

**Deploy and verify:**
```bash
kubectl logs -n stackrox collector-xxxxx | grep openat
```

**Check stats:**
```bash
curl localhost:8080/metrics | grep openat
```

Should see:
```
rox_collector_event_times_us_total{event_type="openat",event_dir=">"} 1234
rox_collector_event_times_us_total{event_type="openat",event_dir="<"} 5678
```

---

## Verifier Constraints and Pitfalls

BPF verifier enforces strict safety guarantees. Understanding its limitations prevents hours of debugging.

### Instruction Limit

**Limit:** 1 million instructions per program (kernel >= 5.2).

**Symptom:**
```
libbpf: load bpf program failed: Invalid argument
libbpf: -- BEGIN DUMP LOG --
libbpf: processed 1000001 insns (limit 1000000)
libbpf: -- END DUMP LOG --
```

**Solution:** Tail calls.

**Example:** execve exit handler exceeded limit collecting 27 parameters. Split into 3 programs:

```c
SEC("tp_btf/sys_exit")
int BPF_PROG(execve_x, struct pt_regs *regs, long ret) {
    // Collect params 1-14
    auxmap__store_s64_param(auxmap, ret);
    // ... 13 more params ...

    // Tail to continuation
    bpf_tail_call(ctx, &extra_syscall_calls, T1_EXECVE_X);
    return 0;
}

SEC("tp_btf/sys_exit")
int BPF_PROG(t1_execve_x, struct pt_regs *regs, long ret) {
    // Collect params 15-20
    // ... 6 params ...

    bpf_tail_call(ctx, &extra_syscall_calls, T2_EXECVE_X);
    return 0;
}

SEC("tp_btf/sys_exit")
int BPF_PROG(t2_execve_x, struct pt_regs *regs, long ret) {
    // Collect params 21-27
    // ... 7 params ...

    auxmap__finalize_event_header(auxmap);
    auxmap__submit_event(auxmap);
    return 0;
}
```

### Loop Bounds

**Limit:** Verifier must prove loops terminate. Unbounded loops rejected.

**Symptom:**
```
libbpf: back-edge from insn 342 to 256
```

**Solution:** Bounded loops with `#pragma unroll` or explicit counters.

**Bad:**
```c
for (struct task_struct *p = task->parent; p != NULL; p = p->parent) {
    // Verifier can't prove termination
}
```

**Good:**
```c
#pragma unroll
for (struct task_struct *p = task->parent; cnt < MAX_DEPTH; p = p->parent) {
    cnt++;
    if (p == NULL) break;
    // Process p
}
```

**ROX-31971:** Some verifiers fail even with `#pragma unroll`, looping infinitely during verification. Workaround: reduce `MAX_DEPTH` or restructure logic.

### Stack Size

**Limit:** 512 bytes (kernel < 5.2) or 8KB (kernel >= 5.2).

**Symptom:**
```
libbpf: combined stack size of 4 programs is 9216 bytes
```

**Solution:** Use per-CPU maps for large buffers.

**Bad:**
```c
char buf[4096];
bpf_probe_read_user(buf, sizeof(buf), ptr);
```

**Good:**
```c
struct auxiliary_map *auxmap = auxmap__get();  // Per-CPU map
bpf_probe_read_user(auxmap->data, MAX_SIZE, ptr);
```

### Helper Call Verification

**Issue:** Verifier tracks pointer state. After certain operations, it may "forget" a pointer is valid.

**Symptom:**
```
R1 invalid mem access 'inv'
```

**Example:** Ring buffer pointer invalidated after complex logic.

**Solution (ROX-31971):** Use auxiliary map approach instead of direct ring buffer reserve:

```c
// Instead of:
void *data = bpf_ringbuf_reserve(&ringbuf, size, 0);
// ... complex logic ...
bpf_ringbuf_submit(data, 0);  // Verifier may reject

// Use:
struct auxiliary_map *auxmap = auxmap__get();
// ... complex logic ...
bpf_ringbuf_output(&ringbuf, auxmap->data, auxmap->payload_pos, 0);
```

### CO-RE Relocations

**Issue:** Field offsets differ across kernel versions. CO-RE handles this, but requires BTF.

**Symptom:**
```
libbpf: failed to find BTF for extern 'task_struct' [16] section
```

**Solution:** Ensure BTF available (`/sys/kernel/btf/vmlinux`). Use `bpf_core_field_exists()` for optional fields:

```c
if (bpf_core_field_exists(inode->i_ctime)) {
    BPF_CORE_READ_INTO(&time, inode, i_ctime);
} else {
    // Kernel 6.6+ moved to __i_ctime
    struct inode___v6_6 *inode_v6_6 = (void *)inode;
    BPF_CORE_READ_INTO(&time, inode_v6_6, __i_ctime);
}
```

### Complexity Limits

**Issue:** Verifier complexity analysis can fail even if instruction count OK.

**Symptom:**
```
libbpf: the BPF verifier is unhappy: verifier log exceeds buffer size
```

**ROX-24938:** Container-Optimized OS (COS) verifier rejected reaper logic. Solution: reduce `MAX_HIERARCHY_TRAVERSE` from 128 to 60.

**ROX-31971:** Clang > 19 generates different code patterns, hitting new verifier limits. Solution: adjust tail call boundaries, simplify conditional logic.

### Common Pitfalls

**1. Reading user pointers without bounds:**
```c
// Bad:
char *user_str = (char *)extract__syscall_argument(regs, 0);
while (*user_str) { ... }  // Unbounded

// Good:
unsigned long user_str = extract__syscall_argument(regs, 0);
auxmap__store_charbuf_param(auxmap, user_str, MAX_PATH, USER);  // Bounded
```

**2. Forgetting NULL checks:**
```c
// Bad:
struct task_struct *parent = BPF_CORE_READ(task, parent);
pid_t ppid = BPF_CORE_READ(parent, pid);  // May crash if parent == NULL

// Good:
struct task_struct *parent = BPF_CORE_READ(task, parent);
if (parent) {
    pid_t ppid = BPF_CORE_READ(parent, pid);
}
```

**3. Mixing kernel and user pointers:**
```c
// Bad:
unsigned long ptr = extract__syscall_argument(regs, 0);
struct foo *f = (struct foo *)ptr;
int val = f->field;  // WRONG: ptr is userspace

// Good:
unsigned long ptr = extract__syscall_argument(regs, 0);
struct foo f;
bpf_probe_read_user(&f, sizeof(f), (void *)ptr);
int val = f.field;
```

**4. Large inline functions:**

Inlining can explode instruction count. Use `__noinline` for large helpers (requires kernel >= 5.8):

```c
__noinline static int parse_complex_struct(...) {
    // ... 500 instructions ...
}
```

### Debugging Verifier Failures

**1. Enable verifier log:**
```bash
echo 1 > /proc/sys/kernel/bpf_stats_enabled
```

**2. Check dmesg for full log:**
```bash
dmesg | grep bpf
```

**3. Use bpftool to inspect loaded programs:**
```bash
bpftool prog show
bpftool prog dump xlated id <id>  # Show translated instructions
bpftool prog dump jited id <id>   # Show JIT assembly
```

**4. Simplify incrementally:**

Comment out sections until verifier passes, then re-enable to isolate issue.

**5. Check kernel version:**

Older kernels have stricter verifier. Collector requires >= 5.8 for CO-RE, but >= 5.13 recommended for better verifier.

---

## Summary

**CO-RE BPF driver architecture:**
- **Attached programs** on `tp_btf/sys_enter`, `tp_btf/sys_exit`, scheduler tracepoints
- **Tail call dispatch** via `BPF_MAP_TYPE_PROG_ARRAY` indexed by syscall ID
- **158 syscall handlers** capturing network, process, file, permission events
- **Variable-size events** built in per-CPU auxiliary maps, submitted to ring buffers
- **ppm_evt_hdr** format with TLV parameters consumed by libscap/libsinsp
- **Full event flow** from kernel tracepoint → BPF → ring buffer → libscap → libsinsp → SystemInspector → NetworkSignalHandler/ProcessSignalHandler → ConnTracker

**Key takeaways:**
- Tail calls are essential for complex syscall monitoring (work around 1M instruction limit)
- Auxiliary maps solve variable-size event challenges with verifier
- CO-RE relocations enable single binary across kernel versions
- Verifier limits require careful loop bounds, stack usage, pointer tracking
- NetworkSignalHandler and ProcessSignalHandler bridge kernel events to collector's connection/process tracking

**Related documentation:**
- [falcosecurity-libs.md](./falcosecurity-libs.md) - High-level overview and integration
- [lib/system.md](./lib/system.md) - SystemInspector abstraction boundary
- [ROX-31971](https://issues.redhat.com/browse/ROX-31971) - Verifier complexity fixes
- [ROX-24938](https://issues.redhat.com/browse/ROX-24938) - COS verifier workarounds
- [ROX-18856](https://issues.redhat.com/browse/ROX-18856) - getsockopt for async connect status

**Source tree:**
- `falcosecurity-libs/driver/modern_bpf/programs/attached/` - Tracepoint entry points
- `falcosecurity-libs/driver/modern_bpf/programs/tail_called/events/syscall_dispatched_events/` - Per-syscall handlers
- `falcosecurity-libs/driver/modern_bpf/maps/maps.h` - Tail tables and global variables
- `falcosecurity-libs/driver/ppm_events_public.h` - Event codes and header format
- `falcosecurity-libs/userspace/libscap/` - Ring buffer polling and event parsing
- `falcosecurity-libs/userspace/libsinsp/` - Event enrichment and container metadata
- `collector/lib/NetworkSignalHandler.cpp` - Network event consumption
- `collector/lib/ProcessSignalHandler.cpp` - Process event consumption
