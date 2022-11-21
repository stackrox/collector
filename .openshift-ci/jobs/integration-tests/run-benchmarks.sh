#!/usr/bin/env bash

set -eo pipefail

BRANCH="$(jq -r '.extra_refs[0].base_ref' <(echo "$JOB_SPEC"))"

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
else
    # perf_table=$(cat benchmark.md)

    # export PERF_TABLE="$perf_table"

    # hub-comment -template-file "${BASELINE}/performance-comment-template.tpl"
    #
    # TODO: add github commenting
    echo "Would comment on PR"
fi
