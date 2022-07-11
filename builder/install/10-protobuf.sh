#!/usr/bin/env bash

set -e

cd third_party/protobuf
cp LICENSE "${LICENSE_DIR}/protobuf-${PROTOBUF_VERSION}"

mkdir cmake-build && cd cmake-build
cmake ../cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_SHARED_LIBS=OFF \
    -Dprotobuf_BUILD_TESTS=OFF
cmake --build . --target install ${NPROCS:+-j ${NPROCS}}
