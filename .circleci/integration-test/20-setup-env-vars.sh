#!/usr/bin/env bash
set -eo pipefail

collection_method=$1
vm_type=$2
offline=$3
image_family=$4
image_name=$5
dockerized=$6
BUILD_NUM=$7

COLLECTOR_REPO="quay.io/rhacs-eng/collector"
if [[ -f pr-metadata/labels/ci-run-against-ubi && "$dockerized" != "true" ]]; then
    COLLECTOR_REPO="quay.io/rhacs-eng/collector-test-cpaas"
fi

TEST_NAME="${collection_method}-${vm_type}-${image_family}"

cat >> "$BASH_ENV" <<- EOF
  export COLLECTION_METHOD="${collection_method}"
  export COLLECTOR_OFFLINE_MODE="${offline}"
  export GCLOUD_INSTANCE="collector-ci-${TEST_NAME}-${BUILD_NUM}"
  export GCLOUD_OPTIONS="--ssh-key-file=${GCP_SSH_KEY_FILE}"
  export IMAGE_FAMILY="${image_family}"
  export IMAGE_NAME="${image_name}"
  export REMOTE_HOST_TYPE=gcloud
  export TEST_NAME="${TEST_NAME}"
  export VM_CONFIG="${vm_type}.${image_family}"
  export VM_TYPE="${vm_type}"
  export COLLECTOR_REPO="${COLLECTOR_REPO}"
  export USE_VALGRIND=$VALGRIND_INTEGRATION_TESTS
  export USE_HELGRIND=$HELGRIND_INTEGRATION_TESTS
EOF

if [[ "$dockerized" == "true" ]]; then
    cat >> "$BASH_ENV" <<- EOF
      export COLLECTOR_IMAGE="${COLLECTOR_REPO}:${COLLECTOR_TAG}-dockerized"
EOF
fi

if "${CI_ROOT}/pr_has_label.sh" ci-benchmark-syscall-latency; then
    cat >> "$BASH_ENV" <<- EOF
    export MEASURE_SYSCALL_LATENCY=true
    export STOP_TIMEOUT=60
EOF
fi
