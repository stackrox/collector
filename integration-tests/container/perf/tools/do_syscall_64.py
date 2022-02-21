#!/usr/bin/env python3
import signal
import functools
from bcc import BPF
from bcc.syscall import syscalls
from collections import defaultdict
import time
import os
import json
import argparse


g_stats = defaultdict(list)


def record_stats(cpu, data, size, bpf, output):
    if not bpf:
        raise RuntimeError("None bpf passed to record_stats")

    event = bpf['latency_events'].event(data)
    if output == "-":
        print(f"{event.id} {event.delta}")
    else:
        try:
            s_name = syscalls[event.id].decode()
            g_stats[s_name].append(event.delta)
        except KeyError:
            g_stats[event.id].append(event.delta)


def lost(count):
    print(f"lost {count}")


def signal_handler(bpf, output, *args):
    """
    Since this tool is run via docker, we need to handle docker stop
    calls, and output our results before exiting. docker will send a
    SIGTERM, which gives us some time to write out output before docker will
    forcibly kill the container.
    """
    bpf.detach_kprobe(event="do_syscall_64")
    bpf.detach_kretprobe(event="do_syscall_64")

    if output != '-':
        with open(output, 'w+') as o:
            json.dump(g_stats, o)

    os._exit(0)


def source_path(name: str) -> str:
    return os.path.join(
        os.path.dirname(__file__),
        "bpf",
        name
    )


def capture(output: str):
    bpf = BPF(src_file=source_path("do_syscall_64_latency.bpf.c"))

    # By using functools.partial() here we can populate the signal handler with additional
    # args that it needs, without haivng extra global state.
    handler = functools.partial(signal_handler, bpf, output)

    signal.signal(signal.SIGTERM, handler)
    signal.signal(signal.SIGINT, handler)

    bpf.attach_kprobe(event="do_syscall_64", fn_name="enter_probe")

    # maxactive sets the number of parallel kretprobe events that
    # may be processed at a time. Without this, we drop a large
    # number of return events.
    bpf.attach_kretprobe(event="do_syscall_64",
                         fn_name="exit_probe", maxactive=1024)

    # we're processing many millions of events, so make sure the perf
    # buffer is large enough.
    bpf['latency_events'].open_perf_buffer(functools.partial(record_stats, bpf=bpf, output=output),
                                           page_cnt=2048, lost_cb=lost)

    while True:
        try:
            bpf.perf_buffer_poll()
        except KeyboardInterrupt:
            break


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--output', '-o', default='/tmp/data.json', help=('Where to write the data. '
                                                          'Use a hyphen ("-") to output to stdout, in a machine-readable form. '
                                                          'Otherwise a json file is written to the location provided.'))

    args = parser.parse_args()
    capture(args.output)
