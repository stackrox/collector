#!/usr/bin/env bash
set -eo pipefail

make -C "${SOURCE_ROOT}/kernel-modules" build-container
docker tag "$BUILD_CONTAINER_TAG" build-kernel-modules-default

shopt -s nullglob

for f in ~/kobuild-tmp/custom-flavors/versions.*; do
  flavor="$(basename "$f")"
  flavor="${flavor#versions\.}"
  make -C "${SOURCE_ROOT}/kernel-modules" "build-container-${flavor}"
  docker tag "${BUILD_CONTAINER_TAG}-${flavor}" "build-kernel-modules-${flavor}"
done
