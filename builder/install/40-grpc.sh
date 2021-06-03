#!/usr/bin/env bash

set -e

CXXFLAGS=""
if [ -f /etc/redhat-release ]; then
	CXXFLAGS="-Wno-error=class-memaccess -Wno-ignored-qualifiers -Wno-stringop-truncation -Wno-cast-function-type"
fi

if [ -n "${CPAAS_BUILD}" ]; then
    tar xzf grpc-${GRPC_VERSION}.tar.gz
    cd grpc-${GRPC_VERSION}
else
    git clone -b "$GRPC_REVISION" --depth 1 https://github.com/grpc/grpc
    cd grpc
    git submodule update --init
fi
cp NOTICE.txt "${LICENSE_DIR}/grpc-${GRPC_REVISION}"
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

if [ -n "${CPAAS_BUILD}" ]; then
    cd ../../third_party/googletest
    mkdir -p cmake/build
    cd cmake/build
    cmake -DCMAKE_BUILD_TYPE=Release ../..
    make -j "${NPROCS:-2}"
    make install
fi
