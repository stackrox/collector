#!/usr/bin/env bash

set -eo pipefail

CI_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"
# shellcheck source=SCRIPTDIR/../../scripts/lib.sh
source "${CI_ROOT}/scripts/lib.sh"

BRANCH="$(get_branch)"

function run_benchmarks() {
    make -C ansible BUILD_TYPE=ci benchmarks
}

function save_artifacts() {
    cp "integration-tests/perf.json" "${ARTIFACT_DIR}/${JOB_NAME_SAFE}-perf.json"
    cp -r "integration-tests/container-logs/" "${ARTIFACT_DIR}"

    if [[ -d "integration-tests/performance-logs" ]]; then
        cp -r "integration-tests/performance-logs" "${ARITFACT_DIR}"
    fi
}

run_benchmarks || exit_code=$?
save_artifacts

if [ "${exit_code}" -ne 0 ]; then
    exit "${exit_code}"
fi

BASELINE=.openshift-ci/scripts/baseline

"${BASELINE}"/main.py --test integration-tests/perf.json \
    | sort \
    | awk -f "${BASELINE}"/format.awk > benchmark.md

if [[ "$BRANCH" == "master" ]]; then
    "${BASELINE}"/main.py --update integration-tests/perf.json
elif ! is_openshift_CI_rehearse_PR; then
    # only post the benchmark results if we're on a collector PR, as opposed to
    # an openshift/release PR or on master.
    perf_table=$(cat benchmark.md)

    pr_id="$(get_pr_details | jq -r .id)"

    export PERF_TABLE="$perf_table"
    export CIRCLE_BRANCH="$BRANCH"
    export CIRCLE_PULL_REQUEST="https://github.com/stackrox/collector/pull/${pr_id}"

    hub-comment -template-file "${BASELINE}/performance-comment-template.tpl"
fi
