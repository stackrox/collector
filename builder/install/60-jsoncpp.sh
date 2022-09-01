#!/usr/bin/env bash

set -e

cd third_party/jsoncpp

cp LICENSE "${LICENSE_DIR}/jsoncpp-${JSONCPP_REVISION}"

mkdir cmake-build
cd cmake-build
cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-fPIC ..
cmake --build . --target install ${NPROCS:+-j ${NPROCS}}
