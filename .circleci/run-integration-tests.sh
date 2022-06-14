#!/usr/bin/env bash
set -eo pipefail

remote_host_type=$1
BRANCH=$2
BUILD_NUM=$3

echo "Running tests with image '${COLLECTOR_IMAGE}'"

function performance_platform_supported() {
    if [[ "${VM_TYPE}" == "flatcar" || "${VM_TYPE}" == "cos" ]]; then
        return 1
    fi
    return 0
}

function integration_tests_with_measurements() {
    COLLECTOR_BCC_COMMAND="/tools/do_syscall_64.py -o /tmp/baseline.json" make -C "${SOURCE_ROOT}" integration-tests-baseline
    COLLECTOR_BCC_COMMAND="/tools/do_syscall_64.py -o /tmp/benchmark.json" make -C "${SOURCE_ROOT}" integration-tests-benchmark

    make -C "${SOURCE_ROOT}" ci-integration-tests
}

function copy_from_vm() {
    filename=$1
    destination=$2

    if [[ "$remote_host_type" == "local" ]]; then
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
        mkdir -p "${SOURCE_ROOT}/integration-tests/performance-logs"
        integration_tests_with_measurements
        exit_code=$?

        copy_from_vm "/tmp/baseline.json" "${SOURCE_ROOT}/integration-tests/performance-logs/baseline-${IMAGE_FAMILY}-${COLLECTION_METHOD}.json"
        copy_from_vm "/tmp/benchmark.json" "${SOURCE_ROOT}/integration-tests/performance-logs/benchmark-${IMAGE_FAMILY}-${COLLECTION_METHOD}.json"
    else
        make -C "${SOURCE_ROOT}" ci-all-tests
        exit_code=$?
    fi

    return $exit_code
}

exit_code=0

if [[ $remote_host_type != "local" ]]; then
    run_tests || exit_code=$?
    cp "${SOURCE_ROOT}/integration-tests/perf.json" "${WORKSPACE_ROOT}/${TEST_NAME}-perf.json"
else
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

[[ -z "$BRANCH" ]] || gsutil cp "${SOURCE_ROOT}/integration-tests/integration-test-report.xml" "gs://stackrox-ci-results/circleci/collector/${BRANCH}/$(date +%Y-%m-%d)-${BUILD_NUM}/"
