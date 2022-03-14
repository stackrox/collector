#!/usr/bin/env bash
set -eo pipefail

docker build \
    --build-arg COLLECTOR_VERSION="${COLLECTOR_VERSION}" \
    --build-arg REDHAT_USERNAME="${REDHAT_USERNAME}" \
    --build-arg REDHAT_PASSWORD="${REDHAT_PASSWORD}" \
    --build-arg USE_VALGRIND="${BUILD_USE_VALGRIND}" \
    --build-arg ADDRESS_SANITIZER="${SANITIZER_TESTS}" \
    --build-arg COLLECTOR_APPEND_CID="${COLLECTOR_APPEND_CID}" \
    -t "collector-test-cpaas:${COLLECTOR_VERSION}" \
    --build-arg CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
    -f "${SOURCE_ROOT}/collector/container/Dockerfile.ubi" \
    "${SOURCE_ROOT}"
