#!/usr/bin/env bash

set -x

copy_secret() {
    local NAME="$1"
    local DEST="$2"
    local PERMS="$3"

    cp "/tmp/secret/stackrox-collector-e2e-tests/$NAME" "$DEST"
    chmod "$PERMS" "$DEST"
}

# to ensure that the locale is consistent between GCP VMs
# we need to set this here. If there are issues with the
# locale, there can be some additional logging which
# breaks the integration tests.
export LC_ALL=C

# JOB_ID is used as a random suffix to distinguish VMs
export JOB_ID="${PROW_JOB_ID:0:8}"

# Most of these are used by the integration tests themselves as well
# as to create and configure the GCP VMs
export GCP_SSH_KEY_FILE="$HOME/.ssh/google_compute_engine"
export GCLOUD_ZONE="us-central1-a"
export GCLOUD_PROJECT="stackrox-ci"
export GCLOUD_INSTANCE="collector-osci-${COLLECTION_METHOD}-${IMAGE_FAMILY}-${JOB_ID}"
export GCLOUD_OPTIONS="--ssh-key-file=${GCP_SSH_KEY_FILE}"
export VM_CONFIG="${VM_TYPE}.${IMAGE_FAMILY}"
export COLLECTOR_REPO="quay.io/rhacs-eng/collector"

export REMOTE_HOST_TYPE=ssh
export SSH_ADDRESS="${GCLOUD_INSTANCE}.${GCLOUD_ZONE}.${GCLOUD_PROJECT}"
export SSH_KEY_PATH="${GCP_SSH_KEY_FILE}"

# TODO: make change ci user on GCP vms
export GCLOUD_USER="circleci"
export SSH_USER="circleci"
if [[ "$VM_TYPE" == "flatcar" || "$VM_TYPE" =~ "coreos" ]]; then
    GCLOUD_USER="core"
    SSH_USER="core"
fi

IMAGE_TAG="$(make tag)"

export COLLECTOR_IMAGE="${COLLECTOR_REPO}:${IMAGE_TAG}-osci"

# Ensure that all secrets are available in the environment
shopt -s nullglob
for cred in /tmp/secret/**/[A-Z]*; do
    export "$(basename "$cred")"="$(cat "$cred")"
done

mkdir -p "$(dirname "${GCP_SSH_KEY_FILE}")"
chmod 0700 "$(dirname "${GCP_SSH_KEY_FILE}")"

copy_secret GCP_SSH_KEY "${GCP_SSH_KEY_FILE}" 0600
copy_secret GCP_SSH_KEY_PUB "${GCP_SSH_KEY_FILE}.pub" 0600
