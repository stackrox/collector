#!/usr/bin/env bash

set -e

cd third_party/prometheus-cpp

cat LICENSE > "${LICENSE_DIR}/prometheus-${PROMETHEUS_CPP_REVISION}"

mkdir cmake-build
cd cmake-build
cmake -DENABLE_TESTING=OFF -DUSE_THIRDPARTY_LIBRARIES=NO -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Release ../
cmake --build . --target install ${NPROCS:+-j ${NPROCS}}
