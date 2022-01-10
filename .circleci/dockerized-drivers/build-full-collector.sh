#!/usr/bin/env bash
set -euo pipefail

COLLECTOR_REPO="$1"

# Get the module version from the collector image and pass it as a build-arg
docker create --name collector "${COLLECTOR_REPO}/collector:${COLLECTOR_TAG}-slim"
docker cp collector:/kernel-modules/MODULE_VERSION.txt ./MODULE_VERSION.txt
module_version="$(cat ./MODULE_VERSION.txt)"
rm -f ./MODULE_VERSION.txt
docker rm collector

# Copy all drivers into the new image
DOCKER_BUILDKIT=1 \
BUILDKIT_PROGRESS=plain \
docker build \
	--tag "${COLLECTOR_REPO}/collector:${COLLECTOR_TAG}-dockerized" \
	--build-arg COLLECTOR_TAG="${COLLECTOR_TAG}-slim" \
	--build-arg DRIVERS_TAG="${COLLECTOR_DRIVERS_TAG}" \
	--build-arg MODULE_VERSION="${module_version}" \
	-f "${SOURCE_ROOT}/kernel-modules/dockerized/Dockerfile.collector" \
	"${SOURCE_ROOT}/kernel-modules/dockerized"

docker push "${COLLECTOR_REPO}/collector:${COLLECTOR_TAG}-dockerized"
