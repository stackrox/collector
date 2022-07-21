#!/usr/bin/env bash

set -x

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
export GCLOUD_INSTANCE="collector-osci-${COLLECTION_METHOD}-${VM_TYPE}-${IMAGE_FAMILY}-${JOB_ID}"
export GCLOUD_OPTIONS="--ssh-key-file=${GCP_SSH_KEY_FILE}"
export GCLOUD_USER="circleci"
export REMOTE_HOST_TYPE=gcloud
export VM_CONFIG="${VM_TYPE}.${IMAGE_FAMILY}"
export COLLECTOR_REPO="quay.io/rhacs-eng/collector"

# TODO: remove this once rebased against image-push functionality (to use image built in this PR)
export COLLECTOR_IMAGE="${COLLECTOR_REPO}:3.9.0"

# Ensure that all secrets are available in the environment
shopt -s nullglob
for cred in /tmp/secret/**/[A-Z]*; do
    export "$(basename "$cred")"="$(cat "$cred")"
done
