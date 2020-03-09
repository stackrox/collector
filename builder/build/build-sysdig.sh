#!/usr/bin/env bash
set -e

set -ux
mkdir -p /sysdig-build
cd /sysdig-build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_DRIVER=OFF \
    -DUSE_BUNDLED_DEPS=OFF \
    /sysdig-src
make -j "${NPROCS:-2}"
strip --strip-unneeded ./userspace/libsinsp/libsinsp-wrapper.so
