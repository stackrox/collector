#!/usr/bin/env bash
set -e

set -ux
mkdir -p /sysdig-build
cd /sysdig-build
cmake -DCMAKE_BUILD_TYPE=Profile -DBUILD_DRIVER=OFF \
    -DUSE_BUNDLED_DEPS=OFF \
    -DNO_LINK_GRPC_LIBS=ON \
    -DCMAKE_C_FLAGS_PROFILE="-ggdb -g -O2 -fno-inline -finline-functions-called-once -finline-small-functions -fno-strict-aliasing -DNDEBUG" \
    -DCMAKE_CXX_FLAGS_PROFILE="-ggdb -g -O2 -fno-inline -finline-functions-called-once -finline-small-functions -fno-strict-aliasing -DNDEBUG" \
    /sysdig-src
make -j "${NPROCS:-2}"
strip --strip-unneeded ./userspace/libsinsp/libsinsp-wrapper.so
