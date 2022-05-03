#!/usr/bin/env bash
set -euo pipefail

DRIVER_REPO="$1"
PARALLEL_BUILDS="$2"
NODE_INDEX="$3"
GIT_REF="$4"

# Empty bundles directory, still needed by docker to build the image
mkdir -p "${STACKROX_ROOT}/bundles"

DOCKER_BUILDKIT=1 \
    BUILDKIT_PROGRESS=plain \
    docker build \
    --build-arg BRANCH="${GIT_REF}" \
    --build-arg REDHAT_SUBSCRIPTION_ORG_ID="${REDHAT_SUBSCRIPTION_ORG_ID}" \
    --build-arg REDHAT_SUBSCRIPTION_ACTIVATION_KEY="${REDHAT_SUBSCRIPTION_ACTIVATION_KEY}" \
    --build-arg USE_KERNELS_FILE="true" \
    --build-arg CACHE_REPO="${DRIVER_REPO}" \
    --build-arg CACHE_TAG="${COLLECTOR_DRIVERS_CACHE}" \
    --target task-master \
    --tag "${DRIVER_REPO}/collector-drivers:${COLLECTOR_DRIVERS_TAG}-${NODE_INDEX}" \
    -f "${SOURCE_ROOT}/kernel-modules/dockerized/Dockerfile" \
    "${STACKROX_ROOT}"

docker create --name task-master "${DRIVER_REPO}/collector-drivers:${COLLECTOR_DRIVERS_TAG}-${NODE_INDEX}"
docker cp task-master:/build-tasks "${WORKSPACE_ROOT}"

# Split tasks by kernels being used (prevent downloading the same bundle twice)
awk '{ print $1 }' "${WORKSPACE_ROOT}/build-tasks" | uniq > "${WORKSPACE_ROOT}/required-kernels"
num_tasks="$(wc -l < "${WORKSPACE_ROOT}/required-kernels")"
shard_size=$(((num_tasks - 1) / PARALLEL_BUILDS + 1))

echo "Total number of kernels to build: ${num_tasks}"
echo "Kernels per shard: ${shard_size}"

split -d -l "${shard_size}" "${WORKSPACE_ROOT}/required-kernels" "${WORKSPACE_ROOT}/task-shard-"
