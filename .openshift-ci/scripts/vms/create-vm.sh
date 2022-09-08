#!/usr/bin/env bash

set -eo pipefail

CI_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"

# shellcheck source=SCRIPTDIR/../lib.sh
source "${CI_ROOT}/scripts/lib.sh"

# shellcheck source=SCRIPTDIR/provision.sh
source "${CI_ROOT}/scripts/vms/provision.sh"

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

    mkdir -p "$(dirname "${GCP_SSH_KEY_FILE}")"
    chmod 0700 "$(dirname "${GCP_SSH_KEY_FILE}")"

    copy_secret_to_file GCP_SSH_KEY "${GCP_SSH_KEY_FILE}" 0600
    copy_secret_to_file GCP_SSH_KEY_PUB "${GCP_SSH_KEY_FILE}.pub" 0600

    setupGCPVM "$GCP_VM_NAME" "$GCP_VM_TYPE" "$GCP_IMAGE_FAMILY" "$GCP_IMAGE_NAME" "$GCP_SSH_KEY_FILE" "$GDOCKER_USER" "$GDOCKER_PASS"
}

main \
    "${GCLOUD_INSTANCE}" \
    "${VM_TYPE}" \
    "${IMAGE_FAMILY}" \
    "${IMAGE_NAME}" \
    "${GCP_SSH_KEY_FILE}" \
    "$(get_secret_content QUAY_RHACS_ENG_RO_USERNAME)" \
    "$(get_secret_content QUAY_RHACS_ENG_RO_PASSWORD)"
