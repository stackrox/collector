#!/bin/bash
set -e

# This script runs the netperf tcp_crr test via cillium/kubenetbench
# with a user configured number of streams.

die() {
    echo >&2 "$@"
    exit 1
}

log() {
    echo "$*" >&2
}

artifacts_dir="$1"
knb_base_dir="$2"

[[ -n "$artifacts_dir" && -n "$knb_base_dir" ]] \
    || die "Usage: $0 <artifacts-dir> <knb-base-dir>"

log "teardown knb-monitor"
kubectl delete ds/knb-monitor

# $knb_dir contains test results that may be useful
#rm -rf "${knb_base_dir}"
