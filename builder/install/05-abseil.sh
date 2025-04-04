#!/usr/bin/env bash

set -e

cd third_party/abseil-cpp

cp LICENSE "${LICENSE_DIR}/Abseil-${ABSEIL_VERSION}"

mkdir cmake-build
cd cmake-build
cmake .. \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_CXX_STANDARD=17 \
    -DABSL_PROPAGATE_CXX_STD=ON

cmake --build . --target install ${NPROCS:+-j ${NPROCS}}
