#!/usr/bin/env bash

set -exo pipefail

main() {
    local GCP_VM_NAME="$1"
    shift
    local GCP_VM_TYPE="$1"
    shift
    local GCP_IMAGE_FAMILY="$1"
    shift
    local GCP_IMAGE_NAME="$1"
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

    setupGCPVM "$GCP_VM_NAME" "$GCP_VM_TYPE" "$GCP_IMAGE_FAMILY" "$GCP_IMAGE_NAME" "$GDOCKER_USER" "$GDOCKER_PASS"
}

main \
    "${GCLOUD_INSTANCE}" \
    "${VM_TYPE}" \
    "${IMAGE_FAMILY}" \
    "${IMAGE_NAME}" \
    "${QUAY_RHACS_ENG_RO_USERNAME}" \
    "${QUAY_RHACS_ENG_RO_PASSWORD}"
