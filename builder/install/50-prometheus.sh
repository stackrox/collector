#!/usr/bin/env bash

set -eu

source builder/install/cmake.sh

cd third_party/prometheus-cpp

cat LICENSE > "${LICENSE_DIR}/prometheus-${PROMETHEUS_CPP_REVISION}"

cmake_wrap "prometheus" "${OUTPUT_DIR}" \
    -DENABLE_TESTING=OFF \
    -DUSE_THIRDPARTY_LIBRARIES=NO
