#!/usr/bin/env bash
set -euo pipefail

DRIVER_REPO="$1"
GIT_REF="$2"
NODE_INDEX="$3"

IMAGE_TAG="${DRIVER_REPO}/collector-drivers:${COLLECTOR_DRIVERS_TAG}-${NODE_INDEX}"

push_with_retry() {
    local image_tag="$1"

    for ((retries = 5; retries > 0; retries--)); do
        if docker push "${image_tag}"; then
            echo "Successfully pushed ${image_tag}"
            break
        fi
        echo >&2 "Failed to push ${image_tag}"
    done

    if ((retries == 0)); then
        return 1
    fi

    return 0
}

# No bundles to build, nothing left to do
if [[ -z "$(ls -A "${STACKROX_ROOT}/bundles")" ]]; then
    exit 0
fi

docker build \
    --build-arg BRANCH="${GIT_REF}" \
    --build-arg REDHAT_SUBSCRIPTION_ORG_ID="${REDHAT_SUBSCRIPTION_ORG_ID}" \
    --build-arg REDHAT_SUBSCRIPTION_ACTIVATION_KEY="${REDHAT_SUBSCRIPTION_ACTIVATION_KEY}" \
    --build-arg CACHE_REPO="${DRIVER_REPO}" \
    --build-arg CACHE_TAG="${COLLECTOR_DRIVERS_CACHE}" \
    --tag "${IMAGE_TAG}" \
    -f "${SOURCE_ROOT}/kernel-modules/dockerized/Dockerfile" \
    "${STACKROX_ROOT}"

push_with_retry "${IMAGE_TAG}"
exit $?
