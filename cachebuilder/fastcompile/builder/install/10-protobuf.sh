#!/usr/bin/env bash
PATH=/usr/lib/ccache:$PATH
set -e

cd
PROTOBUF_VERSION=3.6.1
curl -L "https://github.com/protocolbuffers/protobuf/releases/download/v${PROTOBUF_VERSION}/protobuf-cpp-${PROTOBUF_VERSION}.tar.gz" | tar xz
cd "protobuf-${PROTOBUF_VERSION}"
./autogen.sh
./configure --with-zlib
make -j 6
sudo make install
sudo ldconfig
