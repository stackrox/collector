#!/usr/bin/env bash
set -eo pipefail

remote_host_type=$1
BRANCH=$2
BUILD_NUM=$3

echo "Running tests with image '${COLLECTOR_IMAGE}'"

function integration_tests_with_measurements() {
    COLLECTOR_BCC_COMMAND="/tools/do_syscall_64.py -o /tmp/baseline.json" make -C "${SOURCE_ROOT}" integration-tests-baseline
    COLLECTOR_BCC_COMMAND="/tools/do_syscall_64.py -o /tmp/benchmark.json" make -C "${SOURCE_ROOT}" integration-tests-benchmark

    make -C "${SOURCE_ROOT}" integration-tests-repeat-network integration-tests integration-tests-report || exit_code=$?

    return $exit_code
}

function integration_tests_no_measurements() {
    make -C "${SOURCE_ROOT}" integration-tests-repeat-network \
                             integration-tests-missing-proc-scrape \
                             integration-tests-image-label-json \
                             integration-tests-baseline \
                             integration-tests-benchmark \
                             integration-tests \
                             integration-tests-report
}

function copy_from_vm() {
    filename=$1
    destination=$2

    # We want the shell to expand $GCLOUD_OPTIONS here, so they're treated as individual options
    # instead of a single string.
    # shellcheck disable=SC2086
    gcloud compute scp $GCLOUD_OPTIONS "${GCLOUD_USER}@${GCLOUD_INSTANCE}:${filename}" "${destination}"
}

function run_tests() {
    exit_code=0

    if [[ "${MEASURE_DRIVER_PERFORMANCE}" == "true" ]]; then
        mkdir "${SOURCE_ROOT}/integration-tests/performance-logs"
        echo "user: ${QUAY_STACKROX_IO_RO_USERNAME}"
        docker login -u "${QUAY_STACKROX_IO_RO_USERNAME}" -p "${QUAY_STACKROX_IO_RO_PASSWORD}" quay.io
        integration_tests_with_measurements
        exit_code=$?

        if [[ "$remote_host_type" == "local" ]]; then
            cp "/tmp/baseline.json" "${SOURCE_ROOT}/integration-tests/performance-logs/baseline-${COLLECTION_METHOD}.json"
            cp "/tmp/benchmark.json" "${SOURCE_ROOT}/integration-tests/performance-logs/benchmark-${COLLECTION_METHOD}.json"
        else
            copy_from_vm "/tmp/baseline.json" "${SOURCE_ROOT}/integration-tests/performance-logs/baseline-${COLLECTION_METHOD}.json"
            copy_from_vm "/tmp/benchmark.json" "${SOURCE_ROOT}/integration-tests/performance-logs/benchmark-${COLLECTION_METHOD}.json"
        fi
    else
        integration_tests_no_measurements
        exit_code=$?
    fi

    return $exit_code
}

exit_code=0

if [[ "${MEASURE_DRIVER_PERFORMANCE}" == "true" ]]; then
    mkdir "${SOURCE_ROOT}/integration-tests/performance-logs"
fi

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
