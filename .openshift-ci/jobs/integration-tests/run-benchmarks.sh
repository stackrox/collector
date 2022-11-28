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

exit_code=0

run_benchmarks || exit_code=$?
save_artifacts

if ((exit_code)); then
    exit "${exit_code}"
fi

BASELINE=.openshift-ci/scripts/baseline

echo "$(get_pr_details) | jq"

jq -s 'flatten' integration-tests/perf.json > integration-tests/perf-all.json

"${BASELINE}"/main.py --test integration-tests/perf-all.json \
    | sort \
    | awk -f "${BASELINE}"/format.awk > benchmark.md

if [[ "$BRANCH" == "master" ]]; then
    "${BASELINE}"/main.py --update integration-tests/perf-all.json
elif ! is_openshift_CI_rehearse_PR; then
    # only post the benchmark results if we're on a collector PR, as opposed to
    # an openshift/release PR or on master.
    perf_table=$(cat benchmark.md)

    # use `html_url` rather than `url` to ensure compatibility with hub-comment
    pr_url="$(get_pr_details | jq -r .html_url)"

    export PERF_TABLE="$perf_table"
    export CIRCLE_BRANCH="$BRANCH"
    export CIRCLE_PULL_REQUEST="${pr_url}"

    echo "Posting perf results to ${CIRCLE_PULL_REQUEST}"

    hub-comment -template-file "${BASELINE}/performance-comment-template.tpl"
fi
