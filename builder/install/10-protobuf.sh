#!/usr/bin/env bash

set -e

curl -L "https://github.com/protocolbuffers/protobuf/releases/download/v${PROTOBUF_VERSION}/protobuf-cpp-${PROTOBUF_VERSION}.tar.gz" | tar xz
cd "protobuf-${PROTOBUF_VERSION}"
./autogen.sh
./configure --with-zlib CXXFLAGS=-fPIC
make -j ${NPROCS}
make install
ldconfig
