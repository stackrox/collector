#!/usr/bin/env bash
set -eo pipefail

image_repos=(
  "${DOCKER_REPO}/collector-test-cpaas"
  "${QUAY_REPO}/collector-test-cpaas"
)
image_tags=(
  "${COLLECTOR_VERSION}"
  "${COLLECTOR_VERSION}-slim"
)
for repo in "${image_repos[@]}"; do
  for tag in "${image_tags[@]}"; do
    image="${repo}:${tag}"
    docker image tag "collector-test-cpaas:${COLLECTOR_VERSION}" "${image}"
    docker push "${image}"
  done
done
