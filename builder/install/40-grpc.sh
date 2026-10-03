#!/usr/bin/env bash

set -eu

source builder/install/cmake.sh

export CXXFLAGS="-Wno-error=class-memaccess -Wno-ignored-qualifiers -Wno-stringop-truncation -Wno-cast-function-type -Wno-attributes"

cd third_party/grpc

cp NOTICE.txt "${LICENSE_DIR}/grpc-${GRPC_REVISION}"

cmake_wrap "grpc" "${OUTPUT_DIR}" \
    -DgRPC_PROTOBUF_PROVIDER=package \
    -DgRPC_PROTOBUF_PACKAGE_TYPE=CONFIG \
    -DgRPC_ZLIB_PROVIDER=package \
    -DgRPC_CARES_PROVIDER=package \
    -DgRPC_SSL_PROVIDER=package \
    -DgRPC_RE2_PROVIDER=package \
    -DgRPC_ABSL_PROVIDER=package \
    -DgRPC_BUILD_GRPC_CSHARP_PLUGIN=OFF \
    -DgRPC_BUILD_GRPC_NODE_PLUGIN=OFF \
    -DgRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN=OFF \
    -DgRPC_BUILD_GRPC_PHP_PLUGIN=OFF \
    -DgRPC_BUILD_GRPC_PYTHON_PLUGIN=OFF \
    -DgRPC_BUILD_GRPC_RUBY_PLUGIN=OFF \
    -DgRPC_DOWNLOAD_ARCHIVES=OFF \
    -DgRPC_INSTALL=ON \
    -DCMAKE_CXX_STANDARD=17
