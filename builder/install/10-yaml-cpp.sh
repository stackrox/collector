#!/usr/bin/env bash

set -e

cd third_party/yaml-cpp
cp LICENSE "${LICENSE_DIR}/yaml-cpp-${YAMLCPP_VERSION}"

cmake -B build/ \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
    -DYAML_CPP_BUILD_CONTRIB=OFF \
    -DYAML_CPP_BUILD_TOOLS=OFF \
    -DYAML_BUILD_SHARED_LIBS=OFF \
    -DYAML_CPP_INSTALL=ON
cmake --build build --target install ${NPROCS:+-j ${NPROCS}}
