#!/usr/bin/env bash

function cmake_wrap() {
    set -x
    PKG_NAME="$1"
    shift
    OUTPUT_DIR="$1/${PKG_NAME}"
    shift

    mkdir -p "${OUTPUT_DIR}"
    cmake -S . -B "${OUTPUT_DIR}" \
        -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        "$@"
    cmake --build "${OUTPUT_DIR}" --target install ${NPROCS:+-j ${NPROCS}}
}
