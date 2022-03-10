#!/usr/bin/env bash

set -eo pipefail

TOOL_PID=0

function exit_trap() {
    if [[ $TOOL_PID -ne 0 ]]; then
        kill -INT $TOOL_PID
        wait $TOOL_PID
    fi
    exit 0
}

function preinit() {
    # make sure debugfs is mounted where we expect it. This is required
    # by many of the tools for tracepipe access etc.
    # TODO: investigate why this doesn't propagate through -v /sys:/sys on the host
    mount -t debugfs nodev /sys/kernel/debug || true
}

function run_tool() {
    TOOL="$1"
    shift
    # make sure to background the task so we can set up the pid
    # and handle signals from docker
    eval "$TOOL $* 2>&1 &"
    TOOL_PID=$!

    # This is where we block for the duration of tool execution
    # whilst integration tests/benchmarks are running.
    #
    # When docker tries to stop this container, this is interrupted
    # and the exit_trap is run, which handles cleaning up the tool process.
    wait $!
}

trap exit_trap EXIT

preinit
run_tool "$@"
