#!/usr/bin/env bash

# start workload to generate sched:sched_process_exit on cpu 0
while true; do taskset -c 0 ls &> /dev/null; done &

# start capturing
/sched_process_exit
