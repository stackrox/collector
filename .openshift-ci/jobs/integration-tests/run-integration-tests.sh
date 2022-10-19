#!/usr/bin/env bash
set -eo pipefail

BRANCH="$(jq -r '.extra_refs[0].base_ref' <(echo "$JOB_SPEC"))"

echo "Running tests with image '${COLLECTOR_IMAGE}'"

function run_tests() {
    make -C ansible BUILD_TYPE=ci integration-tests
}

function save_artifacts() {
    cp "integration-tests/perf.json" "${ARTIFACT_DIR}/${JOB_NAME_SAFE}-perf.json"
    cp -r "integration-tests/container-logs/" "${ARTIFACT_DIR}"

    if [[ -d "integration-tests/performance-logs" ]]; then
        cp -r "integration-tests/performance-logs" "${ARITFACT_DIR}"
    fi
}

function propagate_ci_env() {
    if [[ "${OFFLINE,,}" != "false" ]]; then
        export COLLECTOR_OFFLINE_MODE="true"
    fi
}

propagate_ci_env

exit_code=0

if [[ $REMOTE_HOST_TYPE != "local" ]]; then
    run_tests || exit_code=$?
    save_artifacts
    if [ "${exit_code}" -ne 0 ]; then
        exit "${exit_code}"
    fi
else
    # TODO: support local integration tests (or remove if not required)
    sudo cp /proc/sys/kernel/core_pattern /tmp/core_pattern
    echo '/tmp/core.out' | sudo tee /proc/sys/kernel/core_pattern

    run_tests || exit_code=$?

    if [ -f /tmp/core.out ]; then
        sudo chmod 755 /tmp/core.out
    fi

    if [ "${exit_code}" -ne 0 ]; then
        exit "${exit_code}"
    fi
fi

# TODO: make generic CI directory in stackrox-ci-results
if [[ -n "$BRANCH" ]]; then
    for vm_config in integration-tests/container-logs/*; do
        gsutil cp "integration-tests/container-logs/${vm_config}/ebpf/integration-test-report.xml" \
            "gs://stackrox-ci-results/circleci/collector/${BRANCH}/$(date +%Y-%m-%d)-${PROW_JOB_ID}/${vm_config}/integration-test-report-ebpf.xml"

        gsutil cp "integration-tests/container-logs/${vm_config}/kernel-module/integration-test-report.xml" \
            "gs://stackrox-ci-results/circleci/collector/${BRANCH}/$(date +%Y-%m-%d)-${PROW_JOB_ID}/${vm_config}/integration-test-report-kernel-module.xml"
    done
fi
