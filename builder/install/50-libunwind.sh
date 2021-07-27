#!/usr/bin/env bash

set -e

if ldconfig -p | grep -q libunwind; then
    echo "libunwind already installed, exiting."
    exit 0
fi

git clone https://github.com/libunwind/libunwind.git -b "v${LIBUNWIND_VERSION}" --depth 1
cd libunwind
cp COPYING "${LICENSE_DIR}/libunwind-${LIBUNWIND_VERSION}"
autoreconf -i && ./configure && make -j "$(getconf _NPROCESSORS_ONLN)" && make install

