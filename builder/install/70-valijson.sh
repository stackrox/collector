#!/usr/bin/env bash

set -eu

source builder/install/cmake.sh
cd third_party/valijson
cp LICENSE "${LICENSE_DIR}/valijson-${VALIJSON_VERSION}"

cmake_wrap "valijson" "${OUTPUT_DIR}" \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -Dvalijson_BUILD_TESTS=OFF
