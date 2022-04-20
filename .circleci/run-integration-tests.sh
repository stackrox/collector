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

    exit_code=0
    make -C "${SOURCE_ROOT}" integration-tests-repeat-network integration-tests-missing-proc-scrape integration-tests-image-label-json integration-tests integration-tests-report \
        || exit_code=$?

    if [ -f /tmp/core.out ]; then
        sudo chmod 755 /tmp/core.out
    fi

    if [ "${exit_code}" -ne 0 ]; then
        exit "${exit_code}"
    fi
fi

[[ -z "$BRANCH" ]] || gsutil cp "${SOURCE_ROOT}/integration-tests/integration-test-report.xml" "gs://stackrox-ci-results/circleci/collector/${BRANCH}/$(date +%Y-%m-%d)-${BUILD_NUM}/"
