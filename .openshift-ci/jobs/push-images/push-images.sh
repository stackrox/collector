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

full_tags=(
    "${COLLECTOR_VERSION}"
    "${COLLECTOR_VERSION}-latest"
)

base_tags=(
    "${COLLECTOR_VERSION}-slim"
    "${COLLECTOR_VERSION}-base"
)

oc registry login
for repo in "${image_repos[@]}"; do
    registry_rw_login "$repo"

    for tag in "${full_tags[@]}"; do
        image="${repo}/collector:${tag}"
        echo "Pushing image ${image}"
        oc image mirror "${COLLECTOR_FULL}" "${image}"
    done

    for tag in "${base_tags[@]}"; do
        image="${repo}/collector:${tag}"
        echo "Pushing image ${image}"
        oc image mirror "${COLLECTOR_SLIM}" "${image}"
    done
done
