#!/usr/bin/env bash

set -e

export CXXFLAGS="-Wno-error=class-memaccess -Wno-ignored-qualifiers -Wno-stringop-truncation -Wno-cast-function-type -Wno-attributes"

cd third_party/grpc

cp NOTICE.txt "${LICENSE_DIR}/grpc-${GRPC_REVISION}"

# ROX-33133: Remove hardcoded P-256 curve from gRPC (OpenSSL 3.x only) to allow
# OpenSSL to use system crypto-policies defaults, enabling post-quantum key
# exchange (ML-KEM). See: https://github.com/grpc/grpc/issues/23083
patch -p1 < ../../builder/install/grpc-pq-curves.patch

mkdir -p cmake/build
cd cmake/build
cmake \
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
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
    -DgRPC_DOWNLOAD_ARCHIVES=OFF \
    -DgRPC_INSTALL=ON \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_CXX_STANDARD=17 \
    ../..

make ${NPROCS:+-j ${NPROCS}}
make install
