#!/usr/bin/env bash

set -e

CXXFLAGS=""
if [ -f /etc/redhat-release ]; then
	CXXFLAGS="-Wno-error=class-memaccess -Wno-ignored-qualifiers -Wno-stringop-truncation -Wno-cast-function-type"
fi

git clone -b "$GRPC_REVISION" --depth 1 https://github.com/grpc/grpc
cp NOTICE.txt "${LICENSE_DIR}/grpc-${GRPC_REVISION}"
cd grpc
git submodule update --init
mkdir -p cmake/build
cd cmake/build
cmake \
    -DgRPC_CARES_PROVIDER=package \
    -DgRPC_PROTOBUF_PROVIDER=package \
    -DgRPC_SSL_PROVIDER=package \
    -DCMAKE_BUILD_TYPE=Release \
    -DgRPC_INSTALL=ON \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    ../..

make -j "${NPROCS:-2}" CXXFLAGS="${CXXFLAGS}"
make install
