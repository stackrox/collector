#!/usr/bin/env bash
set -exo pipefail

BRANCH="$(jq -r '.extra_refs[0].base_ref' <(echo "$JOB_SPEC"))"

echo "Running tests with image '${COLLECTOR_IMAGE}'"

function performance_platform_supported() {
    if [[ "${VM_TYPE}" == "flatcar" || "${VM_TYPE}" == "cos" ]]; then
        return 1
    fi
    return 0
}

function integration_tests_with_measurements() {
    COLLECTOR_BCC_COMMAND="/tools/do_syscall_64.py -o /tmp/baseline.json" make integration-tests-baseline
    COLLECTOR_BCC_COMMAND="/tools/do_syscall_64.py -o /tmp/benchmark.json" make integration-tests-benchmark

    make ci-integration-tests
}

function copy_from_vm() {
    filename=$1
    destination=$2

    if [[ "$REMOTE_HOST_TYPE" == "local" ]]; then
        cp "$filename" "$destination"
    else
        # We want the shell to expand $GCLOUD_OPTIONS here, so they're treated as individual options
        # instead of a single string.
        # shellcheck disable=SC2086
        gcloud compute scp $GCLOUD_OPTIONS "${GCLOUD_USER}@${GCLOUD_INSTANCE}:${filename}" "${destination}"
    fi
}

function run_tests() {
    exit_code=0

    if [[ "${MEASURE_SYSCALL_LATENCY}" == "true" ]] && performance_platform_supported; then
        mkdir -p "integration-tests/performance-logs"
        integration_tests_with_measurements
        exit_code=$?

        copy_from_vm "/tmp/baseline.json" "integration-tests/performance-logs/baseline-${IMAGE_FAMILY}-${COLLECTION_METHOD}.json"
        copy_from_vm "/tmp/benchmark.json" "integration-tests/performance-logs/benchmark-${IMAGE_FAMILY}-${COLLECTION_METHOD}.json"
    else
        make ci-all-tests
        exit_code=$?
    fi

    return $exit_code
}

function save_artifacts() {
    cp "integration-tests/perf.json" "${ARTIFACT_DIR}/${JOB_NAME_SAFE}-perf.json"
    cp -r "integration-tests/container-logs/" "${ARTIFACT_DIR}"

    if [[ -d "integration-tests/performance-logs" ]]; then
        cp -r "integration-tests/performance-logs" "${ARITFACT_DIR}"
    fi
}

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
[[ -z "$BRANCH" ]] || gsutil cp "integration-tests/integration-test-report.xml" "gs://stackrox-ci-results/circleci/collector/${BRANCH}/$(date +%Y-%m-%d)-${PROW_JOB_ID}/"
[[ -z "$BRANCH" ]] || cp "integration-tests/integration-test-report.xml" "${ARTIFACT_DIR}"
