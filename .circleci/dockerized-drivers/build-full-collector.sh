#!/usr/bin/env bash
set -euo pipefail

COLLECTOR_REPO="$1"

# Copy all drivers into the new image
DOCKER_BUILDKIT=1 \
BUILDKIT_PROGRESS=plain \
docker build \
	--tag "${COLLECTOR_REPO}/collector:${COLLECTOR_TAG}-dockerized" \
	--build-arg COLLECTOR_TAG="${COLLECTOR_TAG}-slim" \
	--build-arg DRIVERS_TAG="${COLLECTOR_DRIVERS_TAG}" \
	-f "${SOURCE_ROOT}/kernel-modules/dockerized/Dockerfile.collector" \
	"${SOURCE_ROOT}/kernel-modules/dockerized"

docker push "${COLLECTOR_REPO}/collector:${COLLECTOR_TAG}-dockerized"
