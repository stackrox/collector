#!/usr/bin/env bash

set -e

cd third_party/protobuf
cp LICENSE "${LICENSE_DIR}/protobuf-${PROTOBUF_VERSION}"

mkdir -p cmake-build && cd cmake-build
cmake -S ../ \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_SHARED_LIBS=OFF \
    -Dprotobuf_BUILD_TESTS=OFF \
    -Dprotobuf_ABSL_PROVIDER=package
cmake --build . --target install ${NPROCS:+-j ${NPROCS}}
