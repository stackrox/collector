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

    if [[ "${COLLECTOR_BUILDER_TAG}" != "cache" ]]; then
        # Push cache only if creating a new builder
        docker push "stackrox/collector-builder:${COLLECTOR_BUILDER_TAG}"

        image="${repo}:${COLLECTOR_BUILDER_TAG}"
        echo "Pushing image ${image}"
        docker image inspect "${image}" > /dev/null
        "${WORKSPACE_ROOT}/go/src/github.com/stackrox/collector/scripts/push-as-manifest-list.sh" "${image}"

    fi

    if [[ "$BRANCH" == "master" ]]; then
        image="${repo}:${COLLECTOR_BUILDER_TAG}"
        echo "Pushing image ${image} as cache"
        docker tag "${image}" "${repo}:cache"
        docker image inspect "${repo}:cache" > /dev/null
        "${WORKSPACE_ROOT}/go/src/github.com/stackrox/collector/scripts/push-as-manifest-list.sh" "${repo}:cache"
    fi
done
