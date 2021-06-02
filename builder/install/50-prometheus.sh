#!/usr/bin/env bash

set -e

tar xzf prometheus-cpp-${PROMETHEUS_CPP_REVISION}.tar.gz
cd prometheus-cpp-${PROMETHEUS_CPP_REVISION}
mkdir -p /usr/local/include
cp -R 3rdparty/civetweb/include /usr/local/include/civetweb

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ../
make -j "${NPROCS:-2}"
make install
ldconfig
