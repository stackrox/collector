#!/usr/bin/env bash
set -e

set -ux
mkdir -p /sysdig-build
cd /sysdig-build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_DRIVER=OFF \
    -DUSE_BUNDLED_DEPS=OFF \
    -DNO_LINK_GRPC_LIBS=ON \
    -DCMAKE_C_FLAGS="-fno-inline -g" \
    -DCMAKE_CXX_FLAGS="-fno-inline -g" \
    /sysdig-src
make -j "${NPROCS:-2}"
strip --strip-unneeded ./userspace/libsinsp/libsinsp-wrapper.so
