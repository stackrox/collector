#!/usr/bin/env bash
set -eo pipefail

GIT_REF="${CIRCLE_BRANCH}"
if [[ -n "${CIRCLE_TAG}" ]]; then
  GIT_REF="${CIRCLE_TAG}"
fi
docker build \
  --build-arg BRANCH="${GIT_REF}" \
  --build-arg REDHAT_USERNAME="${REDHAT_USERNAME}" \
  --build-arg REDHAT_PASSWORD="${REDHAT_PASSWORD}" \
  --tag kernel-builder \
  -f ~/workspace/go/src/github.com/stackrox/collector/kernel-modules/dockerized/Dockerfile \
  ~/workspace/go/src/github.com/stackrox
