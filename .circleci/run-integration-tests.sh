#!/usr/bin/env bash
set -eo pipefail

remote_host_type=$1
BRANCH=$2
BUILD_NUM=$3

echo "Running tests with image '${COLLECTOR_IMAGE}'"

if [[ $remote_host_type != "local" ]]; then
    # I am not sure why there is a difference in the tests that are run locally and in VMs. Perhaps they should be the same
    make -C "${SOURCE_ROOT}" integration-tests-repeat-network integration-tests-baseline integration-tests integration-tests-report

    cp "${SOURCE_ROOT}/integration-tests/perf.json" "${WORKSPACE_ROOT}/${TEST_NAME}-perf.json"
else
    sudo cp /proc/sys/kernel/core_pattern /tmp/core_pattern
    echo '/tmp/core.out' | sudo tee /proc/sys/kernel/core_pattern

    if [[ "${MEASURE_DRIVER_PERFORMANCE}" == "true" ]]; then
        make -C "${SOURCE_ROOT}/integration-tests/container/perf" all

        COLLECTOR_BCC_COMMAND="/tools/do_syscall_64.py -o /tmp/baseline.json" make -C "${SOURCE_ROOT}" integration-tests-baseline
        COLLECTOR_BCC_COMMAND="/tools/do_syscall_64.py -o /tmp/${COLLECTION_METHOD}-benchmark.json" make -C "${SOURCE_ROOT}" integration-tests-benchmark

        make -C "${SOURCE_ROOT}" integration-tests-repeat-network integration-tests integration-tests-report || exit_code=$?

        cp "/tmp/baseline.json" "${WORKSPACE_ROOT}/${TEST_NAME}-baseline.json"
        cp "/tmp/${COLLECTION_METHOD}-benchmark.json" "${WORKSPACE_ROOT}/${TEST_NAME}-${COLLECTION_METHOD}-benchmark.json"
    else
        make -C "${SOURCE_ROOT}" integration-tests-repeat-network integration-tests-missing-proc-scrape integration-tests-image-label-json integration-tests integration-tests-report || exit_code=$?
    fi

    if [ "${exit_code}" -ne 0 ]; then
        exit "${exit_code}"
    fi
fi

[[ -z "$BRANCH" ]] || gsutil cp "${SOURCE_ROOT}/integration-tests/integration-test-report.xml" "gs://stackrox-ci-results/circleci/collector/${BRANCH}/$(date +%Y-%m-%d)-${BUILD_NUM}/"
