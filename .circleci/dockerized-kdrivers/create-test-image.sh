#!/usr/bin/env bash
set -eo pipefail

docker build \
    --tag "stackrox/collector:${COLLECTOR_TAG}-dockerized" \
    --build-arg COLLECTOR_TAG="${COLLECTOR_TAG}-slim" \
    ~/workspace/go/src/github.com/stackrox/collector/kernel-modules/dockerized/tests
