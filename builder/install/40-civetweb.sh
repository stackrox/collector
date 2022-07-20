#!/usr/bin/env bash

set -e

cd third_party/civetweb

cp LICENSE.md "${LICENSE_DIR}/CivetWeb-${CIVETWEB_VERSION}"

mkdir cmake-build
cd cmake-build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local -DCIVETWEB_ENABLE_CXX=ON -DBUILD_SHARED_LIBS:BOOL=NO -DCIVETWEB_BUILD_TESTING=NO ..
cmake --build . --target install ${NPROCS:+-j ${NPROCS}}
