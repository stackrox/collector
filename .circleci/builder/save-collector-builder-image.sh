#!/usr/bin/env bash
set -eo pipefail

if [[ "${COLLECTOR_BUILDER_TAG}" != "cache" ]]; then
  # Push cache only if creating a new builder
  docker push "stackrox/collector-builder:${COLLECTOR_BUILDER_TAG}"
fi
if [[ "$CIRCLE_BRANCH" == "master" ]]; then
  docker tag "stackrox/collector-builder:${COLLECTOR_BUILDER_TAG}" stackrox/collector-builder:cache
  docker push stackrox/collector-builder:cache
fi
