#!/usr/bin/env bash

# This script is a wrapper around bpftrace to do tracing for specified USDT in
# Collector unit tests. It takes a template of tracing script, specifies it for
# a particular tracepoint, runs bpftrace attaching required tracepoints to the
# USDT for the test runner, invokes the test, then extract the resulting stats.
#
# The result will look like this:
#
# $ utilities/tracing/micro-benchmark.sh \
#       BenchmarkConnTracker.FetchEndpointState \
#       fetch_endpoint_state
# {
#   "count": 4,
#   "average": 2003067,
#   "total": 8012271
# }

set -eo pipefail

# name of the unit test to invoke in the form "Benchmark<TestModule>.TestName"
UNIT_TEST="$1"

# name of the tracepoint, there should be two USDT defined in Collector, one
# for start ($TRACEPOINT__start) and one for exit ($TRACEPOINT__exit).
TRACEPOINT="$2"

# path to cmake-build directory
CMAKE_BUILD="$3"

TRACE_SCRIPT=$(mktemp /tmp/trace.bt.XXXXXX)
OUTPUT=$(mktemp /tmp/micro-benchmark.output.XXXXXX)

# shellcheck disable=SC2016 # It doesn't have to be expanded for envsubst
CMAKE_BUILD="${CMAKE_BUILD}" TRACEPOINT="${TRACEPOINT}" \
    envsubst '${CMAKE_BUILD} ${TRACEPOINT}' < \
    "${CMAKE_BUILD}/../utilities/tracing/micro-benchmark-template.bt" > \
    "${TRACE_SCRIPT}"

# attach to all spawned test processes
bpftrace -f json "${TRACE_SCRIPT}" \
                                                        | jq -c 'select(.type == "stats")' \
                                     | jq '.data."@stat"' > "${OUTPUT}" &

# Without jq filtering bpftrace will output a json for every event, like this:
# {"type": "attached_probes", "data": {"probes": 4}}
# {"type": "stats", "data": {"@stat": {"count": 4, "average": 1998382, "total": 7993529}}}

# few moments to attach probes
sleep 1

"${CMAKE_BUILD}/collector/runUnitTests" \
    --gtest_filter="${UNIT_TEST}"

pkill --signal SIGINT bpftrace

# few moments to flush logs
sleep 1

cat "${OUTPUT}"

rm "${TRACE_SCRIPT}"
rm "${OUTPUT}"
