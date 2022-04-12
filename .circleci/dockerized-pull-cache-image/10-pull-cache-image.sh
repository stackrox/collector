#!/usr/bin/env bash
set -eo pipefail

collector_drivers_cache=$1
quay_repo=$2

create_empty_image() {
    docker run --name empty-cache registry.access.redhat.com/ubi8/ubi:8.5 mkdir -p /kernel-modules/
    docker commit empty-cache "${quay_repo}/collector-drivers:$collector_drivers_cache"
    docker rm empty-cache
}

# If we are running with dockerized-no-cache, we create an empty image as base
if [[ "${collector_drivers_cache}" == "no-cache" ]]; then
    create_empty_image
    exit 0
fi

# If we can't pull the cache image, we are either working with no cache or creating the cache itself.
# In any case, we can create the empty image to account for this
if ! docker pull "${quay_repo}/collector-drivers:$collector_drivers_cache"; then
    create_empty_image
fi
