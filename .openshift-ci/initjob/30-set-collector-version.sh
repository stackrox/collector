#!/usr/bin/env bash
set -eo pipefail

shared_env=$1
dockerized_cache_tag=$2
build_tag=$3
TAG=$4
BRANCH=$5
SOURCE_ROOT=$6

check_use_collector_builder_cache() {
    use_cache="true"

    if [[ -n "$TAG" ]]; then
         use_cache="false"
    fi

    if [[ "$BRANCH" == "master" ]]; then
         use_cache="false"
    fi

    if [[ -f pr-metadata/labels/build-builder-image ]]; then
         use_cache="false"
    fi

    if [[ -f pr-metadata/labels/valgrind-unit-tests ]]; then
         use_cache="false"
    fi

    if [[ -f pr-metadata/labels/valgrind-integration-tests ]]; then
         use_cache="false"
    fi

    echo "${use_cache}"
}

COLLECTOR_VERSION="$(make -s -C "${SOURCE_ROOT}" tag)"
COLLECTOR_TAG="${COLLECTOR_VERSION}"
GCLOUD_SSH_FINGERPRINT="$(echo "${GCLOUD_SSH_KEY_PUB}" | awk '{print $2}' | base64 -d | md5sum | awk '{print $1}')"
GCP_SSH_KEY_FILE="${HOME}/.ssh/id_rsa_${GCLOUD_SSH_FINGERPRINT}"

cat >> "$shared_env" <<- EOF
    export COLLECTOR_VERSION="${COLLECTOR_VERSION}"
    export COLLECTOR_TAG="${COLLECTOR_TAG}"
    export GCP_SSH_KEY_FILE="${GCP_SSH_KEY_FILE}"
EOF

if [[ -z "$TAG" && "$BRANCH" != "master" ]]; then
    echo "export COLLECTOR_APPEND_CID=true" >> "$shared_env"
fi

if [[ "$(check_use_collector_builder_cache)" == "true" ]]; then
    echo "export COLLECTOR_BUILDER_TAG=cache" >> "$shared_env"
else
    cat >> "$shared_env" <<- EOF
    export COLLECTOR_BUILDER_TAG="${build_tag}"
    export BUILD_BUILDER_IMAGE=true
EOF
fi

COLLECTOR_DRIVERS_CACHE="$dockerized_cache_tag"
COLLECTOR_DRIVERS_TAG="${build_tag}"

if [[ -f pr-metadata/labels/dockerized-no-cache ]]; then
    COLLECTOR_DRIVERS_CACHE="no-cache"
fi

if [[ -n "$TAG" || "$BRANCH" == "master" ]]; then
    COLLECTOR_DRIVERS_CACHE="$dockerized_cache_tag"
    COLLECTOR_DRIVERS_TAG="$dockerized_cache_tag"
fi

cat >> "$shared_env" <<- EOF
    export COLLECTOR_DRIVERS_CACHE="${COLLECTOR_DRIVERS_CACHE}"
    export COLLECTOR_DRIVERS_TAG="${COLLECTOR_DRIVERS_TAG}"
EOF

if [[ -f pr-metadata/labels/build-qa-containers ]]; then
    echo "export COLLECTOR_QA_TAG=${build_tag}" >> "$shared_env"
fi

cat "$shared_env"
