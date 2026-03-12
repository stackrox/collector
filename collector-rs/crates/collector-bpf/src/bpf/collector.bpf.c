// collector.bpf.c - BPF programs for process and network monitoring
//
// Uses CO-RE (Compile Once Run Everywhere) with vmlinux.h for portability.
// Events are sent to userspace via a BPF ring buffer.

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_endian.h>

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

#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#endif
#ifndef IPPROTO_UDP
#define IPPROTO_UDP 17
#endif
#ifndef AF_INET
#define AF_INET 2
#endif
#ifndef AF_INET6
#define AF_INET6 10
#endif
#ifndef EINPROGRESS
#define EINPROGRESS 115
#endif

// ============================================================
// Shared structures (must match #[repr(C)] Rust structs)
// ============================================================

struct collector_event_header {
    __u32 event_type;
    __u32 pad;
    __u64 timestamp_ns;
    __u32 pid;
    __u32 tid;
    __u32 uid;
    __u32 gid;
};

struct exec_event {
    struct collector_event_header header;
    __u32 ppid;
    __u32 filename_len;
    __u32 args_len;
    __u32 comm_len;
    __u32 cgroup_len;
    __u32 _pad;
    char filename[MAX_FILENAME_LEN];
    char args[MAX_ARGS_LEN];
    char comm[TASK_COMM_LEN];
    char cgroup[MAX_CGROUP_LEN];
};

struct connect_event {
    struct collector_event_header header;
    __u8  saddr[16];
    __u8  daddr[16];
    __u16 sport;
    __u16 dport;
    __u16 family;
    __u8  protocol;
    __u8  _pad;
    __s32 retval;
    __u32 cgroup_len;
    char  cgroup[MAX_CGROUP_LEN];
};

struct exit_event {
    struct collector_event_header header;
    __s32 exit_code;
    __u32 _pad;
};

// ============================================================
// Maps
// ============================================================

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 16 * 1024 * 1024);
} events SEC(".maps");

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

static __always_inline void fill_header(struct collector_event_header *hdr, __u32 type) {
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    __u64 uid_gid = bpf_get_current_uid_gid();

    hdr->event_type = type;
    hdr->pad = 0;
    hdr->timestamp_ns = bpf_ktime_get_boot_ns();
    hdr->pid = pid_tgid >> 32;
    hdr->tid = (__u32)pid_tgid;
    hdr->uid = (__u32)uid_gid;
    hdr->gid = uid_gid >> 32;
}

static __always_inline __u32 read_cgroup(char *buf, __u32 buf_len) {
    struct task_struct *task = (void *)bpf_get_current_task();

    struct css_set *cgroups;
    BPF_CORE_READ_INTO(&cgroups, task, cgroups);
    if (!cgroups) return 0;

    // On cgroup v2, the default cgroup is in dfl_cgrp.
    // On cgroup v1, subsys[0] is used. Try dfl_cgrp first.
    struct cgroup *cgrp;
    BPF_CORE_READ_INTO(&cgrp, cgroups, dfl_cgrp);
    if (!cgrp) {
        struct cgroup_subsys_state *subsys;
        BPF_CORE_READ_INTO(&subsys, cgroups, subsys[0]);
        if (!subsys) return 0;
        BPF_CORE_READ_INTO(&cgrp, subsys, cgroup);
        if (!cgrp) return 0;
    }

    // Walk up the cgroup hierarchy via cgroup->self.parent->cgroup
    // looking for a kernfs node name long enough to contain a container ID.
    // On cgroup v2 with podman/docker, the layout is:
    //   machine.slice / libpod-<64hex>.scope / container
    // We want the "libpod-<64hex>.scope" level.
    #pragma unroll
    for (int i = 0; i < 4; i++) {
        if (!cgrp) return 0;

        struct kernfs_node *kn;
        BPF_CORE_READ_INTO(&kn, cgrp, kn);
        if (!kn) return 0;

        const char *name;
        BPF_CORE_READ_INTO(&name, kn, name);
        if (!name) return 0;

        int ret = bpf_probe_read_kernel_str(buf, buf_len, name);
        if (ret > 64) {
            return ret;
        }

        // Walk to parent cgroup
        struct cgroup_subsys_state *parent_css;
        BPF_CORE_READ_INTO(&parent_css, cgrp, self.parent);
        if (!parent_css) return 0;
        BPF_CORE_READ_INTO(&cgrp, parent_css, cgroup);
    }

    return 0;
}

// ============================================================
// Process exec tracepoint
// ============================================================

SEC("tp_btf/sched_process_exec")
int handle_exec(struct trace_event_raw_sched_process_exec *ctx) {
    __u32 zero = 0;
    struct exec_event *evt = bpf_map_lookup_elem(&exec_heap, &zero);
    if (!evt) return 0;


    fill_header(&evt->header, EVENT_EXEC);

    struct task_struct *task = (void *)bpf_get_current_task();

    // Parent PID
    struct task_struct *parent;
    BPF_CORE_READ_INTO(&parent, task, real_parent);
    BPF_CORE_READ_INTO(&evt->ppid, parent, tgid);

    // comm
    bpf_get_current_comm(evt->comm, sizeof(evt->comm));
    evt->comm_len = TASK_COMM_LEN;

    // filename from the tracepoint data loc
    unsigned int filename_loc = ctx->__data_loc_filename;
    __u16 offset = filename_loc & 0xFFFF;
    __u16 len = (filename_loc >> 16) & 0xFFFF;
    if (len > MAX_FILENAME_LEN - 1) len = MAX_FILENAME_LEN - 1;
    bpf_probe_read_kernel_str(evt->filename, len + 1,
                               (void *)ctx + offset);
    evt->filename_len = len;

    // Process arguments from mm->arg_start to mm->arg_end
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

    bpf_ringbuf_output(&events, evt, sizeof(*evt), 0);
    return 0;
}

// ============================================================
// Process exit tracepoint
// ============================================================

SEC("tp_btf/sched_process_exit")
int handle_exit(struct trace_event_raw_sched_process_template *ctx) {
    // Only report thread group leaders (main thread exit = process exit)
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    if ((__u32)pid_tgid != (pid_tgid >> 32))
        return 0;

    struct exit_event evt = {};
    fill_header(&evt.header, EVENT_EXIT);

    struct task_struct *task = (void *)bpf_get_current_task();
    BPF_CORE_READ_INTO(&evt.exit_code, task, exit_code);

    bpf_ringbuf_output(&events, &evt, sizeof(evt), 0);
    return 0;
}

// ============================================================
// Helper: emit a connect_event from a struct sock
// ============================================================

static __always_inline int emit_sock_event(struct sock *sk, __u32 event_type, __u8 protocol) {
    __u32 zero = 0;
    struct connect_event *evt = bpf_map_lookup_elem(&connect_heap, &zero);
    if (!evt) return 0;

    fill_header(&evt->header, event_type);
    evt->retval = 0;
    evt->protocol = protocol;

    __u16 family;
    BPF_CORE_READ_INTO(&family, sk, __sk_common.skc_family);
    evt->family = family;

    if (family == AF_INET) {
        BPF_CORE_READ_INTO(evt->saddr, sk, __sk_common.skc_rcv_saddr);
        BPF_CORE_READ_INTO(evt->daddr, sk, __sk_common.skc_daddr);
    } else if (family == AF_INET6) {
        BPF_CORE_READ_INTO(evt->saddr, sk, __sk_common.skc_v6_rcv_saddr);
        BPF_CORE_READ_INTO(evt->daddr, sk, __sk_common.skc_v6_daddr);
    } else {
        return 0;
    }

    BPF_CORE_READ_INTO(&evt->sport, sk, __sk_common.skc_num);
    __u16 dport_be;
    BPF_CORE_READ_INTO(&dport_be, sk, __sk_common.skc_dport);
    evt->dport = __bpf_ntohs(dport_be);

    evt->cgroup_len = read_cgroup(evt->cgroup, sizeof(evt->cgroup));

    bpf_ringbuf_output(&events, evt, sizeof(*evt), 0);
    return 0;
}

// TCP state constants
#define TCP_ESTABLISHED  1
#define TCP_SYN_SENT     2
#define TCP_SYN_RECV     3
#define TCP_CLOSE        7

// ============================================================
// Network: TCP connect, accept, close via inet_sock_set_state
// ============================================================

SEC("raw_tracepoint/inet_sock_set_state")
int handle_inet_sock_set_state(struct bpf_raw_tracepoint_args *ctx) {
    // Args: (struct sock *sk, int oldstate, int newstate)
    int oldstate = (int)ctx->args[1];
    int newstate = (int)ctx->args[2];

    struct sock *sk = (struct sock *)ctx->args[0];
    if (!sk) return 0;

    if (newstate == TCP_ESTABLISHED) {
        if (oldstate == TCP_SYN_SENT) {
            // Client connect completed
            return emit_sock_event(sk, EVENT_CONNECT, IPPROTO_TCP);
        } else if (oldstate == TCP_SYN_RECV) {
            // Server accepted
            return emit_sock_event(sk, EVENT_ACCEPT, IPPROTO_TCP);
        }
    } else if (newstate == TCP_CLOSE) {
        return emit_sock_event(sk, EVENT_CLOSE, IPPROTO_TCP);
    }

    return 0;
}

// ============================================================
// Network: UDP connect (ksyscall for portability)
// ============================================================

struct connect_args {
    struct sockaddr *addr;
};

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, __u64);
    __type(value, struct connect_args);
} connect_args_map SEC(".maps");

SEC("ksyscall/connect")
int BPF_KSYSCALL(enter_connect, int fd, struct sockaddr *addr, int addrlen) {
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    struct connect_args args = {};
    args.addr = addr;
    bpf_map_update_elem(&connect_args_map, &pid_tgid, &args, BPF_ANY);
    return 0;
}

SEC("kretsyscall/connect")
int BPF_KRETPROBE(exit_connect, long retval) {
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    struct connect_args *args = bpf_map_lookup_elem(&connect_args_map, &pid_tgid);
    if (!args) return 0;

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

    fill_header(&evt->header, EVENT_CONNECT);
    evt->retval = (int)retval;

    __u16 family;
    bpf_probe_read_user(&family, sizeof(family), &args->addr->sa_family);
    evt->family = family;

    if (family == AF_INET) {
        struct sockaddr_in addr4 = {};
        bpf_probe_read_user(&addr4, sizeof(addr4), args->addr);
        __builtin_memcpy(evt->daddr, &addr4.sin_addr, 4);
        evt->dport = __bpf_ntohs(addr4.sin_port);
    } else if (family == AF_INET6) {
        struct sockaddr_in6 addr6 = {};
        bpf_probe_read_user(&addr6, sizeof(addr6), args->addr);
        __builtin_memcpy(evt->daddr, &addr6.sin6_addr, 16);
        evt->dport = __bpf_ntohs(addr6.sin6_port);
    } else {
        bpf_map_delete_elem(&connect_args_map, &pid_tgid);
        return 0;
    }

    evt->cgroup_len = read_cgroup(evt->cgroup, sizeof(evt->cgroup));

    bpf_ringbuf_output(&events, evt, sizeof(*evt), 0);
    bpf_map_delete_elem(&connect_args_map, &pid_tgid);
    return 0;
}

// ============================================================
// Network: UDP close via fentry
// ============================================================

SEC("fentry/udp_destroy_sock")
int BPF_PROG(handle_udp_destroy_sock, struct sock *sk) {
    return emit_sock_event(sk, EVENT_CLOSE, IPPROTO_UDP);
}

char LICENSE[] SEC("license") = "GPL";
