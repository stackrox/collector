#!/usr/bin/env bash

set -e

CXXFLAGS=""
if [ -f /etc/redhat-release ]; then
	CXXFLAGS="-Wno-error=class-memaccess -Wno-ignored-qualifiers -Wno-stringop-truncation -Wno-cast-function-type"
fi

cd third_party
tar xzf grpc-${GRPC_VERSION}.tar.gz
cd grpc-${GRPC_VERSION}
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
