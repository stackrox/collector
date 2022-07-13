#!/usr/bin/env bash

# shellcheck source=SCRIPTDIR/envbuilder.sh
# shellcheck source=SCRIPTDIR/../../../third_party/stackrox/scripts/ci/lib.sh

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../../../ && pwd)"
source "$ROOT/third_party/stackrox/scripts/ci/lib.sh"

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

which gcloud || true

openshift_ci_import_creds

main \
    "collector-osci-${VM_TYPE}-tests-${JOB_ID}" \
    "${VM_TYPE}" \
    "${IMAGE_FAMILY}" \
    "${IMAGE_NAME}" \
    "/tmp/secret/stackrox-collector-e2e-tests/gcp-ssh-key" \
    "quay-username" \
    "quay-password"
