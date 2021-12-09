#!/usr/bin/env bash
set -eo pipefail

$BRANCH=$1

if [[ "${COLLECTOR_BUILDER_TAG}" != "cache" ]]; then
  # Push cache only if creating a new builder
  docker push "stackrox/collector-builder:${COLLECTOR_BUILDER_TAG}"
fi
if [[ "$BRANCH" == "master" ]]; then
  docker tag "stackrox/collector-builder:${COLLECTOR_BUILDER_TAG}" stackrox/collector-builder:cache
  docker push stackrox/collector-builder:cache
fi
