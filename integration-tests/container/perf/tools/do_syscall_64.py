#!/usr/bin/env python3
import signal
from bcc import BPF
from bcc.syscall import syscalls
from collections import defaultdict
import time
import os
import json
import argparse


parser = argparse.ArgumentParser()
parser.add_argument(
    '--output', '-o', default='/tmp/data.json', help=('Where to write the data. '
                                                      'Use a hyphen ("-") to output to stdout, in a machine-readable form. '
                                                      'Otherwise a json file is written to the location provided.'))

args = parser.parse_args()

if not args.output:
    args.output = "/tmp/data.json"

bpf_text = """
#include <uapi/linux/ptrace.h>
#include <net/sock.h>
#include <bcc/proto.h>

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
"""

bpf = BPF(text=bpf_text)

g_stats = defaultdict(list)


def record_stats(cpu, data, size):
    event = bpf['latency_events'].event(data)
    if args.output == "-":
        print(f"{event.id} {event.delta}")
    else:
        try:
            s_name = syscalls[event.id].decode()
            g_stats[s_name].append(event.delta)
        except KeyError:
            g_stats[event.id].append(event.delta)


bpf.attach_kprobe(event="do_syscall_64", fn_name="enter_probe")

# maxactive sets the number of parallel kretprobe events that
# may be processed at a time. Without this, we drop a large
# number of return events.
bpf.attach_kretprobe(event="do_syscall_64",
                     fn_name="exit_probe", maxactive=1024)


def lost(count):
    print(f"lost {count}")


# we're processing many millions of events, so make sure the perf
# buffer is large enough.
bpf['latency_events'].open_perf_buffer(
    record_stats, page_cnt=2048, lost_cb=lost)


def signal_handler(*args):
    """
    Since this tool is run via docker, we need to handle docker stop
    calls, and output our results before exiting. docker will send a 
    SIGTERM, which gives us some time to write out output before docker will 
    forcibly kill the container.
    """
    bpf.detach_kprobe(event="do_syscall_64")
    bpf.detach_kretprobe(event="do_syscall_64")

    if args.output != '-':
        with open(args.output, 'w+') as o:
            o.write(json.dumps(g_stats))

    os._exit(0)


signal.signal(signal.SIGTERM, signal_handler)
signal.signal(signal.SIGINT, signal_handler)

while True:
    try:
        bpf.perf_buffer_poll()
    except KeyboardInterrupt:
        break
