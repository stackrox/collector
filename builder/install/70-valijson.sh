#!/usr/bin/env bash

set -e

cd third_party/valijson
cp LICENSE "${LICENSE_DIR}/valijson-${VALIJSON_VERSION}"

mkdir cmake-build && cd cmake-build
cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -Dvalijson_INSTALL_HEADERS=ON \
    -Dvalijson_BUILD_TESTS=OFF \
    ..
cmake --build . --target install ${NPROCS:+-j ${NPROCS}}
