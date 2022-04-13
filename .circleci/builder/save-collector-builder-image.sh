#!/usr/bin/env bash
set -eo pipefail

BRANCH=$1

image_repos=(
    "${DOCKER_REPO}/collector-builder"
    "${QUAY_REPO}/collector-builder"
    "${PUBLIC_REPO}/collector-builder"
)

for repo in "${image_repos[@]}"; do
    if [[ "$repo" == "${PUBLIC_REPO}/collector-builder" ]]; then
        # Relogin on quay is needed
        docker login -u "$QUAY_STACKROX_IO_RW_USERNAME" -p "$QUAY_STACKROX_IO_RW_PASSWORD" quay.io
    fi

    image="${repo}:${COLLECTOR_BUILDER_TAG}"

    if [[ "${COLLECTOR_BUILDER_TAG}" != "cache" ]]; then
        # Push cache only if creating a new builder
        echo "Pushing image ${image}"

        # Only Docker image is build at this point, retag it for others.
        # Tagging Docker image doesn't make any changes.
        docker tag "stackrox/collector-builder:${COLLECTOR_BUILDER_TAG}" "${image}"

        docker image inspect "${image}" > /dev/null
        "${WORKSPACE_ROOT}/go/src/github.com/stackrox/collector/scripts/push-as-manifest-list.sh" "${image}"

    fi

    if [[ "$BRANCH" == "master" ]]; then
        echo "Pushing image ${repo}:cache"

        # Only Docker image is build at this point, retag it for others.
        # Tagging Docker image doesn't make any changes.
        docker tag "stackrox/collector-builder:${COLLECTOR_BUILDER_TAG}" "${repo}:cache"

        docker image inspect "${repo}:cache" > /dev/null
        "${SOURCE_ROOT}/scripts/push-as-manifest-list.sh" "${repo}:cache"
    fi
done
