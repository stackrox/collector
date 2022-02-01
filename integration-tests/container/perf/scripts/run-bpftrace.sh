#!/usr/bin/env bash

set -eo pipefail

mount -t debugfs nodev /sys/kernel/debug

bpftrace "$@"
