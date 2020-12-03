#!/usr/bin/env bash

set -e

wget "https://c-ares.haxx.se/download/c-ares-${CARES_VERSION}.tar.gz"
tar -zxf "c-ares-${CARES_VERSION}.tar.gz"
cd "c-ares-${CARES_VERSION}"
cp LICENSE.md "${LICENSE_DIR}/c-ares-${CARES_VERSION}"
./configure --prefix=/usr --with-pic
make -j "${NPROCS:-2}"
make install
