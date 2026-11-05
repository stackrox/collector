#!/usr/bin/env bash

set -eu

source builder/install/cmake.sh

cd third_party/abseil-cpp

cp LICENSE "${LICENSE_DIR}/Abseil-${ABSEIL_VERSION}"

cmake_wrap "abseil-cpp" "${OUTPUT_DIR}" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_CXX_STANDARD=17 \
    -DABSL_PROPAGATE_CXX_STD=ON
