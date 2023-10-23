#!/usr/bin/env bash

# Need debugfs to get the tracepoint id
mount -t debugfs none /sys/kernel/debug

export PERF_BIN
PERF_BIN=$(find /usr/lib/linux-tools -name perf)

# start workload to generate sched:sched_process_exit on cpu 0
while true; do taskset -c 0 ls &> /dev/null; done &

# start capturing
#export SCHED_PROCESS_EXIT_ID

#SCHED_PROCESS_EXIT_ID=$(cat /sys/kernel/debug/tracing/events/sched/sched_process_exit/id)
#/sched_process_exit 10 "${SCHED_PROCESS_EXIT_ID}"

$PERF_BIN stat -e sched:sched_process_exit -C 0 -- sleep 10 2>&1 | grep sched_process_exit | awk '{print $1}'
