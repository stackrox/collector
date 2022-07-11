#!/usr/bin/env bash

set -e

if [ -n "${WITH_RHEL8_RPMS}" ]; then
    # Already installed as an RPM, nothing more to do.
    exit 0
fi

wget "https://c-ares.haxx.se/download/c-ares-${CARES_VERSION}.tar.gz"
tar -zxf "c-ares-${CARES_VERSION}.tar.gz"
cd "c-ares-${CARES_VERSION}"
cp LICENSE.md "${LICENSE_DIR}/c-ares-${CARES_VERSION}"

mkdir cmake-build
cd cmake-build
cmake -DCMAKE_BUILD_TYPE=Release -DCARES_INSTALL=ON -DCMAKE_INSTALL_PREFIX=/usr/local -DCARES_SHARED=OFF -DCARES_STATIC=ON -DCARES_STATIC_PIC=ON ..
cmake --build . --target install ${NPROCS:+-j ${NPROCS}}
