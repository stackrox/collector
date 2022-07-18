#!/usr/bin/env bash

set -e

if [ -n "${WITH_RHEL8_RPMS}" ]; then
    cd valijson
else
    curl -L "https://github.com/tristanpenman/valijson/archive/refs/tags/v${VALIJSON_VERSION}.tar.gz" | tar xz --no-same-owner
    cd "valijson-${VALIJSON_VERSION}"
    cp LICENSE "${LICENSE_DIR}/valijson-${VALIJSON_VERSION}"
fi

mkdir cmake-build && cd cmake-build
cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -Dvalijson_INSTALL_HEADERS=ON \
    -Dvalijson_BUILD_TESTS=OFF \
    ..
make install
ldconfig
