#!/usr/bin/env bash

set -e

if [ "${USE_CCACHE}" = "true" ]; then
    export CC="ccache gcc"
    export CXX="ccache g++"
fi

cd third_party/libbpf

cp LICENSE "${LICENSE_DIR}/libbpf-${LIBBPF_VERSION}"

mkdir src/build
make BUILD_STATIC_ONLY=y OBJDIR=build "LDFLAGS=-Wl,-Bstatic" "CFLAGS=-fPIC" \
     ${NPROCS:+-j ${NPROCS}} -C src install install_uapi_headers
