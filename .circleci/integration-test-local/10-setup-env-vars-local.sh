#!/usr/bin/env bash
set -eo pipefail

vm_config=$1
use_ubi=$2
collection_method=$3

if [[ "$use_ubi" == "true" ]]; then
    export COLLECTOR_REPO="quay.io/rhacs-eng/collector-test-cpaas"
else
    export COLLECTOR_REPO="quay.io/rhacs-eng/collector"
fi

cat >> "$BASH_ENV" <<- EOF
  export REMOTE_HOST_TYPE=local
  export COLLECTION_METHOD="${collection_method}"
  export VM_CONFIG="${vm_config}"
  export COLLECTOR_IMAGE="${COLLECTOR_REPO}:${COLLECTOR_TAG}"
  export USE_VALGRIND="$VALGRIND_INTEGRATION_TESTS"
  export USE_HELGRIND="$HELGRIND_INTEGRATION_TESTS"
EOF

if "${CI_ROOT}/pr_has_label.sh" ci-benchmark-syscall-latency; then
    cat >> "$BASH_ENV" <<- EOF
    export MEASURE_SYSCALL_LATENCY=true
    export STOP_TIMEOUT=60
EOF
fi
