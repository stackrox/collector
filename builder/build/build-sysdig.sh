#!/usr/bin/env bash
set -e

set -ux
mkdir -p /sysdig-build
cd /sysdig-build
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_DRIVER=OFF \
    -DCMAKE_C_FLAGS="-g -ggdb -O0 -fno-inline" \
    -DCMAKE_CXX_FLAGS="-g -ggdb -O0 -fno-inline" \
    -DUSE_BUNDLED_DEPS=OFF \
    -DNO_LINK_GRPC_LIBS=ON \
    /sysdig-src
make -j "${NPROCS:-2}"
strip --strip-unneeded ./userspace/libsinsp/libsinsp-wrapper.so
