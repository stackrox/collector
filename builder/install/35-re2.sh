#!/usr/bin/env bash

set -eu

source builder/install/cmake.sh

cd third_party/re2

cp LICENSE "${LICENSE_DIR}/re2-${RE2_VERSION}"

cmake_wrap "re2" "${OUTPUT_DIR}" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=OFF
