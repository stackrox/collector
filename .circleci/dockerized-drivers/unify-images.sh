#!/usr/bin/env bash
set -euo pipefail

DRIVER_REPO="$1"
SHARDS_COUNT="$2"

# If nothing was built, we don't need to unify anything
if [[ ! -s ~/workspace/build-tasks ]]; then
	if [[ "${COLLECTOR_DRIVERS_TAG}" == "${COLLECTOR_DRIVERS_CACHE}" ]]; then
		exit 0
	fi

	# If the cache and the desired tag are different, we can re-tag and be done
	docker tag \
		"${DRIVER_REPO}/collector-drivers:${COLLECTOR_DRIVERS_CACHE}" \
		"${DRIVER_REPO}/collector-drivers:${COLLECTOR_DRIVERS_TAG}"

	docker push "${DRIVER_REPO}/collector-drivers:${COLLECTOR_DRIVERS_TAG}"

	exit 0
fi

# Create an empty base image
docker pull registry.access.redhat.com/ubi8/ubi:8.5
docker tag registry.access.redhat.com/ubi8/ubi:8.5 "${DRIVER_REPO}/collector-drivers:${COLLECTOR_DRIVERS_TAG}"

for ((i=0; i<SHARDS_COUNT; i++)); do
	PARTIAL_DRIVERS_TAG="${COLLECTOR_DRIVERS_TAG}-${i}"

	if ! docker pull "${DRIVER_REPO}/collector-drivers:${PARTIAL_DRIVERS_TAG}"; then
		# Failed to pull a partial image, either there was nothing to build (and we are fine)
		# or an error occurred (and any drivers missed here will be added in a future run).
		continue
	fi

	DOCKER_BUILDKIT=1 \
	BUILDKIT_PROGRESS=plain \
	docker build \
		--tag "${DRIVER_REPO}/collector-drivers:${COLLECTOR_DRIVERS_CACHE}" \
		--build-arg BASE_TAG="${COLLECTOR_DRIVERS_CACHE}" \
		--build-arg DRIVER_TAG="${PARTIAL_DRIVERS_TAG}" \
		-f "${SOURCE_ROOT}/kernel-modules/dockerized/Dockerfile.merge" \
		"${SOURCE_ROOT}/kernel-modules/dockerized"
done

# Once we merged everything together, we squash the image and push it.
DOCKER_BUILDKIT=1 \
BUILDKIT_PROGRESS=plain \
docker build \
	--tag "${DRIVER_REPO}/collector-drivers:${COLLECTOR_DRIVERS_TAG}" \
	--build-arg DRIVER_REPO="${DRIVER_REPO}" \
	--build-arg DRIVER_TAG="${COLLECTOR_DRIVERS_CACHE}" \
	-f "${SOURCE_ROOT}/kernel-modules/dockerized/Dockerfile.squash" \
	"${SOURCE_ROOT}/kernel-modules/dockerized"

docker push "${DRIVER_REPO}/collector-drivers:${COLLECTOR_DRIVERS_TAG}"
