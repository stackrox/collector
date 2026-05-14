#!/usr/bin/env bash

set -eu

source builder/install/cmake.sh

cd third_party/yaml-cpp
cp LICENSE "${LICENSE_DIR}/yaml-cpp-${YAMLCPP_VERSION}"

cmake_wrap "yaml-cpp" "${OUTPUT_DIR}" \
    -DYAML_CPP_BUILD_CONTRIB=OFF \
    -DYAML_CPP_BUILD_TOOLS=OFF \
    -DYAML_BUILD_SHARED_LIBS=OFF \
    -DYAML_CPP_INSTALL=ON
