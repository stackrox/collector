#!/bin/bash

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
ARTIFACT_ROOT_DIR="$1"

CI_DIRS=()
while IFS='' read -r line; do
    CI_DIRS+=("$line")
done < <(find "$ARTIFACT_ROOT_DIR" -name '*-results.json' -print0 | xargs -0 -I{} dirname {} | sort | uniq)

function create_graph {
    OUTPUT="$1/$2-latency-graph.png"

    echo "[*] generating $OUTPUT"
    "$SCRIPT_DIR"/syscall-latency-graphs.py \
        "$1"/TestSyscallLatencyBaseline-results.json \
        "$1"/TestSyscallLatencyBenchmark-results.json \
        --syscall "$2" \
        --output "$OUTPUT"
}

for dir in "${CI_DIRS[@]}"; do
    echo "[*] Processing $(basename "$dir")"

    create_graph "$dir" "close"
    create_graph "$dir" "read"
done
