#!/usr/bin/env bash

set -eu

source builder/install/cmake.sh

cd third_party/protobuf
cp LICENSE "${LICENSE_DIR}/protobuf-${PROTOBUF_VERSION}"

cmake_wrap "protobuf" "${OUTPUT_DIR}" \
    -DBUILD_SHARED_LIBS=OFF \
    -Dprotobuf_BUILD_TESTS=OFF \
    -Dprotobuf_ABSL_PROVIDER=package
