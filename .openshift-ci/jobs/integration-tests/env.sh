#!/usr/bin/env bash

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"

# shellcheck source=SCRIPTDIR/../../drivers/scripts/lib.sh
source "$ROOT_DIR/drivers/scripts/lib.sh"

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
export GCLOUD_INSTANCE="collector-osci-${COLLECTION_METHOD}-${IMAGE_FAMILY}-${JOB_ID}"
export GCLOUD_OPTIONS="--ssh-key-file=${GCP_SSH_KEY_FILE}"
export REMOTE_HOST_TYPE=gcloud
export VM_CONFIG="${VM_TYPE}.${IMAGE_FAMILY}"
export COLLECTOR_REPO="quay.io/rhacs-eng/collector"

# TODO: make change ci user on GCP vms
export GCLOUD_USER="circleci"
if [[ "$VM_TYPE" == "flatcar" || "$VM_TYPE" =~ "coreos" ]]; then
    GCLOUD_USER="core"
fi

IMAGE_TAG="$(make tag)"

export COLLECTOR_IMAGE="${COLLECTOR_REPO}:${IMAGE_TAG}"

# Ensure that all secrets are available in the environment
shopt -s nullglob
for cred in /tmp/secret/**/[A-Z]*; do
    export "$(basename "$cred")"="$(cat "$cred")"
done

if pr_has_label "skip-integration-tests"; then
    echo "Skipping integration tests for ${VM_CONFIG}"
    exit 0
fi

# only run all integration tests on master, or when the all-integration-tests
# label is added. This is checked in this common env script because it allows
# us to skip pre- and post- steps as well (which source this file)
if is_in_PR_context && ! pr_has_label "all-integration-tests"; then
    if [[ ! "$IMAGE_FAMILY" =~ (rhel-(7|8)|ubuntu-) ]]; then
        echo "Not running integration tests for ${VM_CONFIG}"
        exit 0
    fi
fi
