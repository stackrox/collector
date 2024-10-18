#!/usr/bin/env bash

set -e

## TODO: update builder image to fedora and use builtin ccache

if [ "$USE_CCACHE" = "true" ]; then

    git clone https://github.com/ccache/ccache.git third_party/ccache

    cd third_party/ccache
    git fetch --tags
    git checkout "${CCACHE_VERSION}"

    mkdir cmake-build && cd cmake-build
    cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTING=OFF -DENABLE_DOCUMENTATION=OFF -DREDIS_STORAGE_BACKEND=OFF ..

    make -j "${NPROCS}"
    make install
    ccache --version
fi
