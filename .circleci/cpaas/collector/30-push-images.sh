#!/usr/bin/env bash
set -euxo pipefail

# This script is a subset of .circleci/images/100-push-images.sh
# We should try to unify them in the near future.

image_repos=(
    "${QUAY_REPO}/collector"
)
image_tags=(
    "${COLLECTOR_VERSION}-cpaas"
    "${COLLECTOR_VERSION}-cpaas-latest"
)
for repo in "${image_repos[@]}"; do
    for tag in "${image_tags[@]}"; do
        image="${repo}:${tag}"
        echo "Pushing image ${image}"
        docker image inspect "${image}" > /dev/null
        "${WORKSPACE_ROOT}/go/src/github.com/stackrox/collector/scripts/push-as-manifest-list.sh" "${image}"
    done
done
