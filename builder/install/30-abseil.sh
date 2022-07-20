#!/usr/bin/env bash

set -e

cd third_party/abseil-cpp

cp LICENSE "${LICENSE_DIR}/Abseil-${ABSEIL_VERSION}"

mkdir cmake-build
cd cmake-build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_POSITION_INDEPENDENT_CODE=True ..
cmake --build . --target install ${NPROCS:+-j ${NPROCS}}
