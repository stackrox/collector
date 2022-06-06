#!/usr/bin/env bash

set -e

wget "https://c-ares.haxx.se/download/c-ares-${CARES_VERSION}.tar.gz"
tar -zxf "c-ares-${CARES_VERSION}.tar.gz"
cd "c-ares-${CARES_VERSION}"
cp LICENSE.md "${LICENSE_DIR}/c-ares-${CARES_VERSION}"

mkdir cmake-build
cd cmake-build
cmake -DCMAKE_BUILD_TYPE=Release -DCARES_INSTALL=ON -DCMAKE_INSTALL_PREFIX=/usr -DCARES_SHARED=OFF -DCARES_STATIC=ON -DCARES_STATIC_PIC=ON ..
make -j "${NPROCS:-2}"
make install
