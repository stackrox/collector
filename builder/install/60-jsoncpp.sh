#!/usr/bin/env bash

set -eu

source builder/install/cmake.sh

cd third_party/jsoncpp

cp LICENSE "${LICENSE_DIR}/jsoncpp-${JSONCPP_REVISION}"

cmake_wrap "jsoncpp" "${OUTPUT_DIR}" \
    -DCMAKE_CXX_FLAGS=-fPIC \
    -DJSONCPP_WITH_TESTS=OFF \
    -DJSONCPP_WITH_POST_BUILD_UNITTEST=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_OBJECT_LIBS=OFF
