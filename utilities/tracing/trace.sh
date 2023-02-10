#!/usr/bin/env bash

# This script is a wrapper around bpftrace to do tracing for specified USDT in
# Collector. It takes a template of tracing script, specifies it for a
# particular tracepoint and amount of time to trace, runs bpftrace attaching
# required tracepoints to the newest found process of Collector, then extract
# the resulting stats.
#
# The result will look like this:
#
# $ utilities/tracing/trace.sh 10 fetch_endpoint_state
# {
#   "count": 4,
#   "average": 2003067,
#   "total": 8012271
# }

set -eo pipefail

# tracing interval in seconds
INTERVAL="$1"

# name of the tracepoint, there should be two USDT defined in Collector, one
# for start ($TRACEPOINT__start) and one for exit ($TRACEPOINT__exit).
TRACEPOINT="$2"

TRACE_SCRIPT=$(mktemp /tmp/trace.bt.XXXXXX)

# shellcheck disable=SC2016 # It doesn't have to be expanded for envsubst
INTERVAL="${INTERVAL}" TRACEPOINT="${TRACEPOINT}" \
    envsubst '${INTERVAL} ${TRACEPOINT}' < utilities/tracing/template.bt > "${TRACE_SCRIPT}"

# At the moment there is only one Collector process that could possibly exist,
# but still take the latest one.
bpftrace -f json -p "$(pgrep -n collector)" "${TRACE_SCRIPT}" \
                                                        | jq -c 'select(.type == "stats")' \
                                     | jq '.data."@stat"'

# Without jq filtering bpftrace will output a json for every event, like this:
# {"type": "attached_probes", "data": {"probes": 4}}
# {"type": "stats", "data": {"@stat": {"count": 4, "average": 1998382, "total": 7993529}}}

rm "${TRACE_SCRIPT}"
