#!/usr/bin/env bash
set -eo pipefail

echo "ls 1"
for i in "${WORKSPACE_ROOT}"/*perf.json; do
    echo "Performance data file: $i"
    cat "$i" >> "${WORKSPACE_ROOT}"/all-perf.json
done

jq '. | select(.Metrics.hackbench_avg_time != null) | {kernel: .VmConfig, collection_method: .CollectionMethod, (.TestName): .Metrics.hackbench_avg_time } ' < "${WORKSPACE_ROOT}"/all-perf.json \
        | jq -rs  ' group_by(.kernel) | .[] | group_by(.collection_method) | .[] | add | [.kernel, .collection_method, .baseline_benchmark, .collector_benchmark ] | @csv' \
        > "${WORKSPACE_ROOT}/benchmark.csv"

echo "|Kernel|Method|Without Collector Time (secs)|With Collector Time (secs)|" > "${WORKSPACE_ROOT}/benchmark.md"
echo "|---|---|---|---|" >> "${WORKSPACE_ROOT}/benchmark.md"
sort "${WORKSPACE_ROOT}/benchmark.csv" | awk -v FS="," '{printf "|%s|%s|%s|%s|%s",$1,$2,$3,$4,ORS}' >> "${WORKSPACE_ROOT}/benchmark.md"
