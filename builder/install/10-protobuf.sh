#!/usr/bin/env bash

set -e

curl -L "https://github.com/protocolbuffers/protobuf/releases/download/v${PROTOBUF_VERSION}/protobuf-cpp-${PROTOBUF_VERSION}.tar.gz" | tar xz --no-same-owner
cd "protobuf-${PROTOBUF_VERSION}"
cp LICENSE "${LICENSE_DIR}/protobuf-${PROTOBUF_VERSION}"

mkdir cmake-build && cd cmake-build
cmake ../cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBUILD_SHARED_LIBS=OFF \
    -Dprotobuf_BUILD_TESTS=OFF
make install
ldconfig
