#!/usr/bin/env bash
set -eo pipefail

export PROJECT_DIR=/go/src/github.com/stackrox/collector
cd "$PROJECT_DIR"

collector_version="$(make tag)"
export COLLECTOR_VERSION="$collector_version"

export PUBLIC_REPO=quay.io/stackrox-io
export QUAY_REPO=quay.io/rhacs-eng

shopt -s nullglob
for cred in /tmp/secret/**/[A-Z]*; do
    export "$(basename "$cred")"="$(cat "$cred")"
done

# shellcheck source=SCRIPTDIR/registry_rw_login.sh
source "${PROJECT_DIR}/.openshift-ci/jobs/push-images/registry_rw_login.sh"

image_repos=(
    "${QUAY_REPO}"
    "${PUBLIC_REPO}"
)

declare -A image_tags
image_tags["$COLLECTOR_FULL"]="${COLLECTOR_VERSION}-osci"
#image_tags["$COLLECTOR_BASE"]="${COLLECTOR_VERSION}-base"
#image_tags["$COLLECTOR_SLIM"]="${COLLECTOR_VERSION}-slim"
#image_tags["$COLLECTOR_LATEST"]="${COLLECTOR_VERSION}-latest"

oc registry login
for repo in "${image_repos[@]}"; do
    registry_rw_login "$repo"
    for key in "${!image_tags[@]}"; do
        image="${repo}/collector:${image_tags[$key]}"
        echo "Pushing image ${image}"
        osci_image="$key"
        oc image mirror "$osci_image" "${image}"
    done
done
