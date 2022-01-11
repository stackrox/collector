#!/usr/bin/env bash

set -e

CXXFLAGS=""
if [ -f /etc/redhat-release ]; then
    CXXFLAGS="-Wno-error=class-memaccess -Wno-ignored-qualifiers -Wno-stringop-truncation -Wno-cast-function-type"
fi

if [ -n "${WITH_RHEL8_RPMS}" ]; then
    cd grpc
    cd third_party
    rmdir abseil-cpp protobuf
    mv ../../abseil-cpp .
    mv ../../protobuf .
    cd ..
    CMAKE_FLAGS=(
        -DgRPC_ZLIB_PROVIDER=package
        -DgRPC_BUILD_GRPC_CSHARP_PLUGIN=OFF
        -DgRPC_BUILD_GRPC_NODE_PLUGIN=OFF
        -DgRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN=OFF
        -DgRPC_BUILD_GRPC_PHP_PLUGIN=OFF
        -DgRPC_BUILD_GRPC_PYTHON_PLUGIN=OFF
        -DgRPC_BUILD_GRPC_RUBY_PLUGIN=OFF
    )
else
    git clone -b "$GRPC_REVISION" --depth 1 https://github.com/grpc/grpc
    cd grpc
    git submodule update --init
    CMAKE_FLAGS=(
        -DgRPC_PROTOBUF_PROVIDER=package
    )
fi
cp NOTICE.txt "${LICENSE_DIR}/grpc-${GRPC_REVISION}"
mkdir -p cmake/build
cd cmake/build
cmake \
    "${CMAKE_FLAGS[@]}" \
    -DgRPC_CARES_PROVIDER=package \
    -DgRPC_SSL_PROVIDER=package \
    -DCMAKE_BUILD_TYPE=Release \
    -DgRPC_INSTALL=ON \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    ../..

make -j "${NPROCS:-2}" CXXFLAGS="${CXXFLAGS}"
make install
