#!/usr/bin/env bash

CI_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"
# shellcheck source=SCRIPTDIR/../../scripts/lib.sh
source "${CI_ROOT}/scripts/lib.sh"

# to ensure that the locale is consistent between GCP VMs
# we need to set this here. If there are issues with the
# locale, there can be some additional logging which
# breaks the integration tests.
export LC_ALL=C

# JOB_ID is used as a random suffix to distinguish VMs
export JOB_ID="${PROW_JOB_ID:0:8}"

# Most of these are used by the integration tests themselves as well
# as to create and configure the GCP VMs
export GCP_SSH_KEY_FILE="$HOME/.ssh/GCP_SSH_KEY"
export GCP_PROJECT="stackrox-ci"
export COLLECTOR_REPO="quay.io/rhacs-eng/collector"

IMAGE_TAG="$(make tag)"

export COLLECTOR_IMAGE
CPAAS_TEST=${CPAAS_TEST:-0}

if ((CPAAS_TEST)); then
     COLLECTOR_IMAGE="${COLLECTOR_REPO}:cpaas-${IMAGE_TAG}"
else
     COLLECTOR_IMAGE="${COLLECTOR_REPO}:${IMAGE_TAG}"
fi

import_creds

mkdir -p "$(dirname "${GCP_SSH_KEY_FILE}")"
chmod 0700 "$(dirname "${GCP_SSH_KEY_FILE}")"
copy_secret_to_file GCP_SSH_KEY "${GCP_SSH_KEY_FILE}" 0600
copy_secret_to_file GCP_SSH_KEY_PUB "${GCP_SSH_KEY_FILE}.pub" 0600

if pr_has_label "skip-integration-tests" || pr_has_label "skip-${VM_TYPE}-integration-tests"; then
    echo "Skipping integration tests for ${VM_TYPE}"
    exit 0
fi

# only run all integration tests on master, or when the all-integration-tests
# label is added. This is checked in this common env script because it allows
# us to skip pre- and post- steps as well (which source this file)
if is_in_PR_context && ! pr_has_label "all-integration-tests"; then
    if [[ ! "$VM_TYPE" =~ (rhel|ubuntu) ]]; then
        echo "Not running integration tests for ${VM_TYPE}"
        exit 0
    fi
fi
