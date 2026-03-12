#!/usr/bin/env bash

set -eu

if [ -n "${WITH_RHEL_RPMS:-}" ]; then
    # Already installed as an RPM, nothing more to do.
    exit 0
fi

source builder/install/cmake.sh

cd third_party/c-ares
cp LICENSE.md "${LICENSE_DIR}/c-ares-${CARES_VERSION}"

cmake_wrap "c-ares" "${OUTPUT_DIR}" \
    -DCARES_INSTALL=ON \
    -DCARES_SHARED=OFF \
    -DCARES_STATIC=ON \
    -DCARES_STATIC_PIC=ON \
    -DCARES_BUILD_TOOLS=OFF
