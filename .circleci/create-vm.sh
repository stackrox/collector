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
