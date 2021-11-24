#!/usr/bin/env bash
set -euo pipefail

DRIVER_REPO="$1"
PARALLEL_BUILDS="$2"
NODE_INDEX="$3"
GIT_REF="$4"

# Empty bundles directory, still needed by docker to build the image
mkdir -p ~/workspace/go/src/github.com/stackrox/bundles

DOCKER_BUILDKIT=1 \
BUILDKIT_PROGRESS=plain \
docker build \
	--build-arg BRANCH="${GIT_REF}" \
	--build-arg REDHAT_USERNAME="${REDHAT_USERNAME}" \
	--build-arg REDHAT_PASSWORD="${REDHAT_PASSWORD}" \
	--build-arg USE_KERNELS_FILE="true" \
	--build-arg CACHE_REPO="${DRIVER_REPO}" \
	--build-arg CACHE_TAG="${COLLECTOR_DRIVERS_CACHE}" \
	--target task-master \
	--tag "${DRIVER_REPO}/collector-drivers:${COLLECTOR_DRIVERS_TAG}-${NODE_INDEX}" \
	-f "${SOURCE_ROOT}/kernel-modules/dockerized/Dockerfile" \
	~/workspace/go/src/github.com/stackrox

docker create --name task-master "${DRIVER_REPO}/collector-drivers:${COLLECTOR_DRIVERS_TAG}-${NODE_INDEX}"
docker cp task-master:/build-tasks ~/workspace/

# Split tasks by kernels being used (prevent downloading the same bundle twice)
awk '{ print $1 }' ~/workspace/build-tasks | uniq > ~/workspace/required-kernels
num_tasks="$(wc -l < ~/workspace/required-kernels)"
shard_size=$(((num_tasks - 1) / PARALLEL_BUILDS + 1))

echo "Total number of kernels to build: ${num_tasks}"
echo "Kernels per shard: ${shard_size}"

split -d -l "${shard_size}" ~/workspace/required-kernels ~/workspace/task-shard-
