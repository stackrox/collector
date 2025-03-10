#!/usr/bin/env bash

set -e

if [ -n "${WITH_RHEL_RPMS}" ]; then
    # Already installed as an RPM, nothing more to do.
    exit 0
fi

cd third_party/c-ares
cp LICENSE.md "${LICENSE_DIR}/c-ares-${CARES_VERSION}"

mkdir cmake-build
cd cmake-build
cmake -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
    -DCARES_INSTALL=ON \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCARES_SHARED=OFF \
    -DCARES_STATIC=ON \
    -DCARES_STATIC_PIC=ON \
    -DCARES_BUILD_TOOLS=OFF \
    ..
cmake --build . --target install ${NPROCS:+-j ${NPROCS}}
