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

    struct cgroup_subsys_state *subsys;
    BPF_CORE_READ_INTO(&subsys, cgroups, subsys[0]);
    if (!subsys) return 0;

    struct cgroup *cgrp;
    BPF_CORE_READ_INTO(&cgrp, subsys, cgroup);
    if (!cgrp) return 0;

    struct kernfs_node *kn;
    BPF_CORE_READ_INTO(&kn, cgrp, kn);
    if (!kn) return 0;

    const char *name;
    BPF_CORE_READ_INTO(&name, kn, name);
    if (!name) return 0;

    int ret = bpf_probe_read_kernel_str(buf, buf_len, name);
    return ret > 0 ? ret : 0;
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
// Network: connect (two-phase kprobe/kretprobe)
// ============================================================

struct connect_args {
    struct sockaddr *addr;
    int fd;
};

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, __u64);
    __type(value, struct connect_args);
} connect_args_map SEC(".maps");

SEC("kprobe/__sys_connect")
int kprobe_connect(struct pt_regs *ctx) {
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    struct connect_args args = {};
    args.fd = PT_REGS_PARM1(ctx);
    args.addr = (struct sockaddr *)PT_REGS_PARM2(ctx);
    bpf_map_update_elem(&connect_args_map, &pid_tgid, &args, BPF_ANY);
    return 0;
}

SEC("kretprobe/__sys_connect")
int kretprobe_connect(struct pt_regs *ctx) {
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    struct connect_args *args = bpf_map_lookup_elem(&connect_args_map, &pid_tgid);
    if (!args) return 0;

    int retval = PT_REGS_RC(ctx);
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
    evt->retval = retval;

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
// Network: accept (TCP state transition)
// ============================================================

SEC("tp_btf/inet_sock_set_state")
int handle_inet_sock_set_state(struct trace_event_raw_inet_sock_set_state *ctx) {
    // Only SYN_RECV -> ESTABLISHED = server accepted
    if (ctx->newstate != 1 /* TCP_ESTABLISHED */)
        return 0;
    if (ctx->oldstate != 3 /* TCP_SYN_RECV */)
        return 0;

    __u32 zero = 0;
    struct connect_event *evt = bpf_map_lookup_elem(&connect_heap, &zero);
    if (!evt) return 0;


    fill_header(&evt->header, EVENT_ACCEPT);
    evt->retval = 0;
    evt->family = ctx->family;
    evt->protocol = IPPROTO_TCP;

    if (ctx->family == AF_INET) {
        __builtin_memcpy(evt->saddr, ctx->saddr, 4);
        __builtin_memcpy(evt->daddr, ctx->daddr, 4);
    } else {
        __builtin_memcpy(evt->saddr, ctx->saddr_v6, 16);
        __builtin_memcpy(evt->daddr, ctx->daddr_v6, 16);
    }
    evt->sport = ctx->sport;
    evt->dport = __bpf_ntohs(ctx->dport);

    evt->cgroup_len = read_cgroup(evt->cgroup, sizeof(evt->cgroup));

    bpf_ringbuf_output(&events, evt, sizeof(*evt), 0);
    return 0;
}

// ============================================================
// Network: TCP close
// ============================================================

SEC("kprobe/tcp_close")
int handle_tcp_close(struct pt_regs *ctx) {
    struct sock *sk = (struct sock *)PT_REGS_PARM1(ctx);
    if (!sk) return 0;

    __u32 zero = 0;
    struct connect_event *evt = bpf_map_lookup_elem(&connect_heap, &zero);
    if (!evt) return 0;


    fill_header(&evt->header, EVENT_CLOSE);
    evt->retval = 0;
    evt->protocol = IPPROTO_TCP;

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

// ============================================================
// Network: generic close for UDP sockets
// ============================================================

struct close_args {
    int fd;
};

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, __u64);
    __type(value, struct close_args);
} close_args_map SEC(".maps");

SEC("kprobe/__sys_close")
int kprobe_close(struct pt_regs *ctx) {
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    struct close_args args = {};
    args.fd = PT_REGS_PARM1(ctx);
    bpf_map_update_elem(&close_args_map, &pid_tgid, &args, BPF_ANY);
    return 0;
}

SEC("kretprobe/__sys_close")
int kretprobe_close(struct pt_regs *ctx) {
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    struct close_args *args = bpf_map_lookup_elem(&close_args_map, &pid_tgid);
    if (!args) return 0;

    int retval = PT_REGS_RC(ctx);
    if (retval < 0) {
        bpf_map_delete_elem(&close_args_map, &pid_tgid);
        return 0;
    }

    // Look up fd -> file -> socket -> sk to check if UDP
    struct task_struct *task = (void *)bpf_get_current_task();
    struct files_struct *files;
    BPF_CORE_READ_INTO(&files, task, files);
    if (!files) goto cleanup;

    struct fdtable *fdt;
    BPF_CORE_READ_INTO(&fdt, files, fdt);
    if (!fdt) goto cleanup;

    struct file **fd_array;
    BPF_CORE_READ_INTO(&fd_array, fdt, fd);
    if (!fd_array) goto cleanup;

    struct file *file;
    bpf_probe_read_kernel(&file, sizeof(file), &fd_array[args->fd]);
    if (!file) goto cleanup;

    // Check if it's a socket
    struct inode *inode;
    BPF_CORE_READ_INTO(&inode, file, f_inode);
    if (!inode) goto cleanup;

    // socket_file_ops check: verify via i_mode that it's a socket
    __u16 i_mode;
    BPF_CORE_READ_INTO(&i_mode, inode, i_mode);
    if ((i_mode & 0xF000) != 0xC000) // S_IFSOCK = 0140000, but upper nibble is 0xC
        goto cleanup;

    // Get the socket struct from the inode
    struct socket *socket;
    socket = (struct socket *)((char *)inode + sizeof(struct inode));

    struct sock *sk;
    BPF_CORE_READ_INTO(&sk, socket, sk);
    if (!sk) goto cleanup;

    // Only handle UDP (TCP goes through tcp_close)
    __u8 protocol;
    BPF_CORE_READ_INTO(&protocol, sk, sk_protocol);
    if (protocol != IPPROTO_UDP)
        goto cleanup;

    __u32 zero = 0;
    struct connect_event *evt = bpf_map_lookup_elem(&connect_heap, &zero);
    if (!evt) goto cleanup;


    fill_header(&evt->header, EVENT_CLOSE);
    evt->retval = 0;
    evt->protocol = IPPROTO_UDP;

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
        goto cleanup;
    }

    BPF_CORE_READ_INTO(&evt->sport, sk, __sk_common.skc_num);
    __u16 dport_be;
    BPF_CORE_READ_INTO(&dport_be, sk, __sk_common.skc_dport);
    evt->dport = __bpf_ntohs(dport_be);

    evt->cgroup_len = read_cgroup(evt->cgroup, sizeof(evt->cgroup));

    bpf_ringbuf_output(&events, evt, sizeof(*evt), 0);

cleanup:
    bpf_map_delete_elem(&close_args_map, &pid_tgid);
    return 0;
}

char LICENSE[] SEC("license") = "GPL";
