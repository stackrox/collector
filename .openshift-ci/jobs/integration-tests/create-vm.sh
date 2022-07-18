#!/usr/bin/env bash

# shellcheck source=SCRIPTDIR/envbuilder.sh

set -e
main() {
    local GCP_VM_NAME="$1"
    shift
    local GCP_VM_TYPE="$1"
    shift
    local GCP_IMAGE_FAMILY="$1"
    shift
    local GCP_IMAGE_NAME="$1"
    shift
    local GCP_SSH_KEY_FILE="$1"
    shift
    local GDOCKER_USER="$1"
    shift
    local GDOCKER_PASS="$1"
    shift

    DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"  > /dev/null 2>&1 && pwd)"

    pushd "$DIR"
    source "envbuilder.sh"
    popd

    setupGCPVM "$GCP_VM_NAME" "$GCP_VM_TYPE" "$GCP_IMAGE_FAMILY" "$GCP_IMAGE_NAME" "$GCP_SSH_KEY_FILE" "$GDOCKER_USER" "$GDOCKER_PASS"
}

ls -lah /tmp/secret/stackrox-collector-e2e-tests/

mkdir -p "$(dirname "${GCP_SSH_KEY_FILE}")"
chmod 0700 "$(dirname "${GCP_SSH_KEY_FILE}")"

cp /tmp/secret/stackrox-collector-e2e-tests/GCP_SSH_KEY "${GCP_SSH_KEY_FILE}"
chmod 0600 "${GCP_SSH_KEY_FILE}"

cp /tmp/secret/stackrox-collector-e2e-tests/GCP_SSH_KEY_PUB "${GCP_SSH_KEY_FILE}.pub"
chmod 0600 "${GCP_SSH_KEY_FILE}.pub"

env

QUAY_RHACS_ENG_RO_USERNAME="$(cat /tmp/secret/stackrox-collector-e2e-tests/QUAY_RHACS_ENG_RO_USERNAME)"
QUAY_RHACS_ENG_RO_PASSWORD="$(cat /tmp/secret/stackrox-collector-e2e-tests/QUAY_RHACS_ENG_RO_PASSWORD)"

main \
    "collector-osci-${VM_TYPE}-tests-${JOB_ID}" \
    "${VM_TYPE}" \
    "${IMAGE_FAMILY}" \
    "${IMAGE_NAME}" \
    "${GCP_SSH_KEY_FILE}" \
    "${QUAY_RHACS_ENG_RO_USERNAME}" \
    "${QUAY_RHACS_ENG_RO_PASSWORD}"
