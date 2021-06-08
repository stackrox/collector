#!/usr/bin/env bash

set -e

if [ -n "${CPAAS_BUILD}" ]; then
    cd prometheus-cpp
    cd 3rdparty
    rmdir civetweb googletest
    mv ../../civetweb .
    cp -a ../../googletest .
    cd ..
    cp LICENSE "${LICENSE_DIR}/prometheus-${PROMETHEUS_CPP_REVISION}"
else
    git clone https://github.com/jupp0r/prometheus-cpp.git -b "$PROMETHEUS_CPP_REVISION" --depth 1
    cd prometheus-cpp
    git submodule update --init
fi

mkdir -p /usr/local/include
cp -R 3rdparty/civetweb/include /usr/local/include/civetweb
cat LICENSE 3rdparty/civetweb/LICENSE.md > "${LICENSE_DIR}/prometheus-${PROMETHEUS_CPP_REVISION}"
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ../
make -j "${NPROCS:-2}"
make install
ldconfig
