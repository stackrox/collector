#!/usr/bin/env bash
set -eo pipefail

image_repos=(
    "${DOCKER_REPO}/collector"
    "${QUAY_REPO}/collector"
    "${PUBLIC_REPO}/collector"
)
image_tags=(
    "${COLLECTOR_VERSION}"
    "${COLLECTOR_VERSION}-base"
    "${COLLECTOR_VERSION}-slim"
    "${COLLECTOR_VERSION}-latest"
)
for repo in "${image_repos[@]}"; do
    if [[ "$repo" == "${PUBLIC_REPO}/collector" ]]; then
        docker login -u "$QUAY_STACKROX_IO_RW_USERNAME" -p "$QUAY_STACKROX_IO_RW_PASSWORD" quay.io
    fi
    for tag in "${image_tags[@]}"; do
        image="${repo}:${tag}"
        echo "Pushing image ${image}"
        docker image inspect "${image}" > /dev/null
        "${WORKSPACE_ROOT}/go/src/github.com/stackrox/collector/scripts/push-as-manifest-list.sh" "${image}"
    done
done
