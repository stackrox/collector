#include <bcc/proto.h>
#include <net/sock.h>
#include <uapi/linux/ptrace.h>

struct latency_event {
  u32 id;
  u32 delta;
};

BPF_HASH(ids, u32);
BPF_HASH(start, u32);
BPF_PERF_OUTPUT(latency_events);

// attach to do_syscall_64 and record the current timestamp
// as well as the syscall ID to separate individual syscalls
void enter_probe(struct pt_regs *ctx, u32 arg0) {
  u64 tgid = bpf_get_current_pid_tgid();
  u32 pid = tgid;
  u64 ts = bpf_ktime_get_ns();
  u32 id = arg0;

  start.update(&pid, &ts);
  ids.update(&pid, &id);
}

// process the return from do_syscall_64. This may be called fewer times
// than enter_probe, due to limitations in the way that kprobes/kretprobes are
// processed.
//
// see: https://www.kernel.org/doc/Documentation/kprobes.txt
//
// the probe will be attached with a high maxactive to allow a high number of
// parallel kretprobe executions.
void exit_probe(struct pt_regs *ctx) {
  u64 tgid = bpf_get_current_pid_tgid();
  u32 pid = tgid;

  u64 *ts = start.lookup(&pid);
  u32 *id = ids.lookup(&pid);
  if (ts == 0 || id == 0) {
    return;
  }

  u32 delta = bpf_ktime_get_ns() - *ts;
  start.delete(&pid);
  ids.delete(&pid);

  struct latency_event event;
  event.id = *id;
  event.delta = delta;

  latency_events.perf_submit(ctx, &event, sizeof(struct latency_event));
}