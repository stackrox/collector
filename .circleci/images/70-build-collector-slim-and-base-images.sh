#!/usr/bin/env bash
set -eo pipefail

build_args=(
    --build-arg module_version="$MODULE_VERSION"
    --build-arg collector_version="$COLLECTOR_VERSION"
    --build-arg USE_VALGRIND="$BUILD_USE_VALGRIND"
    --build-arg ADDRESS_SANITIZER="$SANITIZER_TESTS"
)

COLLECTOR_BUILD_CONTEXT="collector/container/rhel"
if [[ "${SANITIZER_TESTS}" == "true" || "${BUILD_USE_VALGRIND}" == "true" ]]; then
    COLLECTOR_BUILD_CONTEXT="collector/container/devel"
    build_args+=(
        --build-arg BASE_REGISTRY="quay.io"
        --build-arg BASE_IMAGE="centos/centos"
        --build-arg BASE_TAG="stream8"
    )
fi

"${SOURCE_ROOT}/collector/container/create-bundle.sh" \
    "$SOURCE_ROOT/collector/container" \
    "$SOURCE_ROOT/${COLLECTOR_BUILD_CONTEXT}"

make -C "$SOURCE_ROOT" container-dockerfile

docker build \
    -t "${QUAY_REPO}/collector:${COLLECTOR_VERSION}-base" \
    -t "${QUAY_REPO}/collector:${COLLECTOR_VERSION}-slim" \
    -t "${PUBLIC_REPO}/collector:${COLLECTOR_VERSION}-base" \
    -t "${PUBLIC_REPO}/collector:${COLLECTOR_VERSION}-slim" \
    "${build_args[@]}" \
    -f "${SOURCE_ROOT}/collector/container/Dockerfile.gen" \
    "$SOURCE_ROOT/${COLLECTOR_BUILD_CONTEXT}"
