#!/usr/bin/env bash
# Run an already-built validator and retain its actual exit status and evidence.
set -euo pipefail

build_dir="${1:?Usage: bash run-corpus.sh BUILD_DIR RESULTS_DIR [gtest arguments...]}"
results_dir="${2:?Usage: bash run-corpus.sh BUILD_DIR RESULTS_DIR [gtest arguments...]}"
shift 2

test -x "$build_dir/collector/test/plugin-replay/ContainerPluginReplayTest"
mkdir -p "$results_dir"
export ROX_COLLECTOR_CONTAINER_PLUGIN_PATH="${ROX_COLLECTOR_CONTAINER_PLUGIN_PATH:-$build_dir/collector/collector-container-plugin.so}"

"$build_dir/collector/test/plugin-replay/ContainerPluginReplayTest" \
    --gtest_repeat="${REPLAY_REPEAT:-1}" \
    --gtest_shuffle --gtest_random_seed="${REPLAY_SEED:-3939}" \
    --gtest_output="xml:$results_dir/results.xml" \
    "$@" 2>&1 | tee "$results_dir/run.log"
