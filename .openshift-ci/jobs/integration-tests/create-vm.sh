#!/usr/bin/env bash

set -eo pipefail

copy_secret() {
    local NAME="$1"
    local DEST="$2"
    local PERMS="$3"

    cp "/tmp/secret/stackrox-collector-e2e-tests/$NAME" "$DEST"
    chmod "$PERMS" "$DEST"
}

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
    # shellcheck source=SCRIPTDIR/envbuilder.sh
    source "envbuilder.sh"
    popd

    mkdir -p "$(dirname "${GCP_SSH_KEY_FILE}")"
    chmod 0700 "$(dirname "${GCP_SSH_KEY_FILE}")"

    copy_secret GCP_SSH_KEY "${GCP_SSH_KEY_FILE}" 0600
    copy_secret GCP_SSH_KEY_PUB "${GCP_SSH_KEY_FILE}.pub" 0600

    setupGCPVM "$GCP_VM_NAME" "$GCP_VM_TYPE" "$GCP_IMAGE_FAMILY" "$GCP_IMAGE_NAME" "$GCP_SSH_KEY_FILE" "$GDOCKER_USER" "$GDOCKER_PASS"
}

main \
    "${GCLOUD_INSTANCE}" \
    "${VM_TYPE}" \
    "${IMAGE_FAMILY}" \
    "${IMAGE_NAME}" \
    "${GCP_SSH_KEY_FILE}" \
    "${QUAY_RHACS_ENG_RO_USERNAME}" \
    "${QUAY_RHACS_ENG_RO_PASSWORD}"
