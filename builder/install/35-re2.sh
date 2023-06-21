#!/usr/bin/env bash

set -e

cd third_party/re2

cp LICENSE "${LICENSE_DIR}/re2-${RE2_VERSION}"

mkdir cmake-build && cd cmake-build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=OFF
cmake --build . --target install ${NPROCS:+-j ${NPROCS}}
