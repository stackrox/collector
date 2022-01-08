#!/usr/bin/env bash
set -eo pipefail

build_args=(
  --build-arg module_version="$MODULE_VERSION"
  --build-arg collector_version="$COLLECTOR_VERSION"
  --build-arg USE_VALGRIND="$BUILD_USE_VALGRIND"
  --build-arg ADDRESS_SANITIZER="$SANITIZER_TESTS"
)

"${SOURCE_ROOT}/collector/container/rhel/create-bundle.sh" \
  "$SOURCE_ROOT/collector/container" \
  "-" \
  "$SOURCE_ROOT/collector/container/rhel"

docker build \
  -t "${DOCKER_REPO}/collector:${COLLECTOR_VERSION}-base" \
  -t "${DOCKER_REPO}/collector:${COLLECTOR_VERSION}-slim" \
  -t "${QUAY_REPO}/collector:${COLLECTOR_VERSION}-base" \
  -t "${QUAY_REPO}/collector:${COLLECTOR_VERSION}-slim" \
  "${build_args[@]}" \
  -f "${SOURCE_ROOT}/collector/container/rhel/Dockerfile" \
  "$SOURCE_ROOT/collector/container/rhel"
