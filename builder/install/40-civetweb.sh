#!/usr/bin/env bash

set -eu

source builder/install/cmake.sh

cd third_party/civetweb

cp LICENSE.md "${LICENSE_DIR}/CivetWeb-${CIVETWEB_VERSION}"

cmake_wrap "civetweb" "${OUTPUT_DIR}" \
    -DCIVETWEB_ENABLE_CXX=ON \
    -DBUILD_SHARED_LIBS:BOOL=NO \
    -DCIVETWEB_BUILD_TESTING=NO \
    -DCIVETWEB_ENABLE_IPV6=NO \
    -DCIVETWEB_ENABLE_SERVER_EXECUTABLE=NO
