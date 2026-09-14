#!/usr/bin/env bash
# Run process and connection benchmarks against explicit Collector images, then
# preserve profiles, workload results, and run context for later comparison.
set -euo pipefail

PERF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CURRENT_WORKTREE="$(git -C "${PERF_DIR}/.." rev-parse --show-toplevel)"
RESULT_DIR="${CURRENT_WORKTREE}/integration-tests/container-logs/core-bpf/TestBenchmarkCollector"
INTEGRATION_TEST_LOG="${CURRENT_WORKTREE}/integration-tests/integration-test.log"
SAMPLER_PID=
COLLECTOR_LOG_LEVEL="${COLLECTOR_LOG_LEVEL:-info}"

if [[ $# -ne 1 || ! $1 =~ ^[[:alnum:]_.-]+$ || $1 == "." || $1 == ".." ]]; then
    printf 'Usage: %s RUN_NAME\n' "$0" >&2
    exit 1
fi

OUTPUT_DIR="${PERF_DIR}/$1"
mkdir -p "$OUTPUT_DIR"

declare -a WORKLOADS=(processes connections)

# Set these to the immutable Quay image references you want to compare.
declare -A IMAGES=(
    [3.25.0]="${COLLECTOR_IMAGE_3_25_0:-quay.io/stackrox-io/collector:3.25.7-3-g284a332178}"
    [master]="${COLLECTOR_IMAGE_MASTER:?Set COLLECTOR_IMAGE_MASTER to the Quay master image}"
    [current]="${COLLECTOR_IMAGE_CURRENT:?Set COLLECTOR_IMAGE_CURRENT to the Quay current-branch image}"
)

stop_sampler() {
    if [[ -n "$SAMPLER_PID" ]]; then
        kill "$SAMPLER_PID" 2>/dev/null || true
        wait "$SAMPLER_PID" 2>/dev/null || true
        SAMPLER_PID=
    fi
}

trap stop_sampler EXIT INT TERM

capture_command() {
    local output=$1
    shift

    {
        printf '$'
        printf ' %q' "$@"
        printf '\n'
        "$@"
    } >> "$output" 2>&1 || true
}

capture_metadata() {
    local destination=$1
    local version=$2
    local workload=$3
    local image=$4
    local metadata="${destination}/metadata.txt"

    {
        printf 'version=%s\n' "$version"
        printf 'workload=%s\n' "$workload"
        printf 'image=%s\n' "$image"
        printf 'capture_started_utc=%s\n' "$(date --utc --iso-8601=seconds)"
        printf 'working_tree=%s\n' "$CURRENT_WORKTREE"
        printf 'perf_frequency=%s\n' "${COLLECTOR_CPU_PROFILE_FREQUENCY:-199}"
    } > "$metadata"

    capture_command "$metadata" uname -a
    capture_command "$metadata" lscpu
    capture_command "$metadata" perf version
    capture_command "$metadata" docker version
    capture_command "$metadata" docker info
    capture_command "$metadata" git -C "$CURRENT_WORKTREE" rev-parse HEAD
    capture_command "$metadata" git -C "$CURRENT_WORKTREE" status --short --branch
    capture_command "$metadata" git -C "$CURRENT_WORKTREE" submodule status

    cp /proc/cpuinfo "${destination}/cpuinfo.txt" 2>/dev/null || true
    cp /proc/meminfo "${destination}/meminfo.txt" 2>/dev/null || true
    cp /proc/cmdline "${destination}/kernel-cmdline.txt" 2>/dev/null || true
    cp /proc/sys/kernel/perf_event_paranoid "${destination}/perf_event_paranoid.txt" 2>/dev/null || true
    cp /proc/sys/kernel/perf_event_max_sample_rate "${destination}/perf_event_max_sample_rate.txt" 2>/dev/null || true

    docker image inspect "$image" > "${destination}/image-inspect.json" 2> "${destination}/image-inspect.err" || true
}

sample_run() {
    local destination=$1
    local sequence=0

    while true; do
        local timestamp
        timestamp=$(date --utc +%s.%N)

        curl --silent --show-error --max-time 2 http://localhost:9090/metrics \
            > "${destination}/prometheus/${sequence}-${timestamp}.prom" 2>/dev/null || \
            rm -f "${destination}/prometheus/${sequence}-${timestamp}.prom"

        while IFS= read -r stats; do
            printf '%s\t%s\n' "$timestamp" "$stats"
        done < <(docker stats --no-stream --format '{{json .}}' \
            collector cpu-profile benchmark-processes benchmark-connections 2>/dev/null || true) \
            >> "${destination}/docker-stats.jsonl"

        sequence=$((sequence + 1))
        sleep 1
    done
}

save_artefacts() {
    local version=$1
    local workload=$2
    local run_source=$3
    local data_destination="${OUTPUT_DIR}/${version}-${workload}.data"
    local root_destination="${OUTPUT_DIR}/root-${version}-${workload}"

    [[ ! -e "$data_destination" ]] || { printf 'Refusing to overwrite %s\n' "$data_destination" >&2; return 1; }
    [[ ! -e "$root_destination" ]] || { printf 'Refusing to overwrite %s\n' "$root_destination" >&2; return 1; }
    [[ -r "${RESULT_DIR}/perf.data" ]] || { printf 'Missing readable profile at %s\n' "${RESULT_DIR}/perf.data" >&2; return 1; }
    [[ -d "${RESULT_DIR}/rootfs" ]] || { printf 'Missing Collector rootfs at %s\n' "${RESULT_DIR}/rootfs" >&2; return 1; }

    cp "${RESULT_DIR}/perf.data" "$data_destination"
    cp -a "${RESULT_DIR}/rootfs" "$root_destination"
    cp -a "${RESULT_DIR}/." "${run_source}/results/"
    cp "$INTEGRATION_TEST_LOG" "${run_source}/integration-test.log" 2>/dev/null || true
    perf buildid-list -i "$data_destination" > "${run_source}/perf-buildids.txt" 2>&1 || true
    perf evlist -i "$data_destination" > "${run_source}/perf-events.txt" 2>&1 || true
    printf 'capture_finished_utc=%s\n' "$(date --utc --iso-8601=seconds)" >> "${run_source}/metadata.txt"
    perf buildid-cache --add "${root_destination}/usr/local/bin/collector"
}

run_version() {
    local version=$1
    local image=$2

    for workload in "${WORKLOADS[@]}"; do
        local run_destination="${OUTPUT_DIR}/${version}-${workload}-artefacts"
        [[ ! -e "$run_destination" ]] || { printf 'Refusing to overwrite %s\n' "$run_destination" >&2; return 1; }
        mkdir -p "${run_destination}/prometheus" "${run_destination}/results"
        capture_metadata "$run_destination" "$version" "$workload" "$image"

        : > "$INTEGRATION_TEST_LOG"
        sample_run "$run_destination" &
        SAMPLER_PID=$!

        set +e
        COLLECTOR_LOG_LEVEL="$COLLECTOR_LOG_LEVEL" \
        COLLECTOR_IMAGE="$image" \
        COLLECTOR_BENCHMARK_WORKLOADS="$workload" \
        COLLECTOR_CPU_PROFILE=true \
        COLLECTOR_CPU_PROFILE_FREQUENCY="${COLLECTOR_CPU_PROFILE_FREQUENCY:-199}" \
        make -C "${CURRENT_WORKTREE}/integration-tests" TestBenchmarkCollector
        local test_status=$?
        set -e

        stop_sampler
        if [[ $test_status -ne 0 ]]; then
            cp "$INTEGRATION_TEST_LOG" "${run_destination}/integration-test.log" 2>/dev/null || true
            printf 'test_exit_status=%d\n' "$test_status" >> "${run_destination}/metadata.txt"
            return "$test_status"
        fi

        printf 'test_exit_status=0\n' >> "${run_destination}/metadata.txt"
        save_artefacts "$version" "$workload" "$run_destination"
    done
}

fold_profile() {
    local version=$1
    local workload=$2
    local profile="${OUTPUT_DIR}/${version}-${workload}.data"
    local folded="${OUTPUT_DIR}/${version}-${workload}.folded"
    local flamegraph="${OUTPUT_DIR}/${version}-${workload}.svg"

    perf script -i "$profile" | stackcollapse-perf.pl > "$folded"
    flamegraph.pl \
        --title "Collector ${version}: ${workload}" \
        --countname samples \
        --width 2400 \
        --minwidth 0.5 \
        "$folded" > "$flamegraph"
}

diff_flamegraph() {
    local version=$1
    local workload=$2
    local baseline="${OUTPUT_DIR}/3.25.0-${workload}.folded"
    local comparison="${OUTPUT_DIR}/${version}-${workload}.folded"
    local output="${OUTPUT_DIR}/3.25.0-vs-${version}-${workload}.svg"

    difffolded.pl -n "$baseline" "$comparison" \
        | flamegraph.pl \
            --title "Collector ${workload}: 3.25.0 vs ${version}" \
            --subtitle "Normalised CPU samples: red = more CPU in ${version}; blue = less" \
            --countname samples \
            --width 2400 \
            --minwidth 0.5 \
            > "$output"
}

generate_graphs() {
    command -v stackcollapse-perf.pl >/dev/null || { printf 'Missing stackcollapse-perf.pl in PATH\n' >&2; return 1; }
    command -v flamegraph.pl >/dev/null || { printf 'Missing flamegraph.pl in PATH\n' >&2; return 1; }
    command -v difffolded.pl >/dev/null || { printf 'Missing difffolded.pl in PATH\n' >&2; return 1; }

    for workload in "${WORKLOADS[@]}"; do
        fold_profile "3.25.0" "$workload"
        fold_profile "master" "$workload"
        fold_profile "current" "$workload"
        diff_flamegraph "master" "$workload"
        diff_flamegraph "current" "$workload"
    done
}

run_version "3.25.0" "${IMAGES[3.25.0]}"
run_version "master" "${IMAGES[master]}"
run_version "current" "${IMAGES[current]}"
generate_graphs
