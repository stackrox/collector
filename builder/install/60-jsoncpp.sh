#!/usr/bin/env bash

set -e

cd third_party/jsoncpp

cp LICENSE "${LICENSE_DIR}/jsoncpp-${JSONCPP_REVISION}"

mkdir cmake-build
cd cmake-build
cmake -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
    -DCMAKE_CXX_FLAGS=-fPIC \
    -DJSONCPP_WITH_TESTS=OFF \
    -DJSONCPP_WITH_POST_BUILD_UNITTEST=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_OBJECT_LIBS=OFF \
    ..
cmake --build . --target install ${NPROCS:+-j ${NPROCS}}
