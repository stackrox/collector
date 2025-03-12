#!/usr/bin/env bash

set -e

cd third_party/valijson
cp LICENSE "${LICENSE_DIR}/valijson-${VALIJSON_VERSION}"

mkdir cmake-build && cd cmake-build
cmake \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -Dvalijson_BUILD_TESTS=OFF \
    ..
cmake --build . --target install ${NPROCS:+-j ${NPROCS}}
