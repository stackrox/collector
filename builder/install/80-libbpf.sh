#!/usr/bin/env bash

set -e

cd third_party/libbpf

cp LICENSE "${LICENSE_DIR}/libbpf-${LIBBPF_VERSION}"

mkdir src/build
make BUILD_STATIC_ONLY=y OBJDIR=build "LDFLAGS=-Wl,-Bstatic" "CFLAGS=-fPIC" \
     ${NPROCS:+-j ${NPROCS}} -C src install install_uapi_headers
