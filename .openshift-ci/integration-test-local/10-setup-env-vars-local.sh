#!/usr/bin/env bash
set -eo pipefail

vm_config=$1
use_ubi=$2
collection_method=$3

if [[ "$use_ubi" == "true" ]]; then
    export COLLECTOR_REPO="stackrox/collector-test-cpaas"
else
    export COLLECTOR_REPO="stackrox/collector"
fi

cat >> "$BASH_ENV" <<- EOF
  export REMOTE_HOST_TYPE=local
  export COLLECTION_METHOD="${collection_method}"
  export VM_CONFIG="${vm_config}"
  export COLLECTOR_IMAGE="${COLLECTOR_REPO}:${COLLECTOR_TAG}"
  export USE_VALGRIND="$VALGRIND_INTEGRATION_TESTS"
  export USE_HELGRIND="$HELGRIND_INTEGRATION_TESTS"
EOF
