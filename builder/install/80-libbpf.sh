#!/usr/bin/env bash

set -eu

cd third_party/libbpf

cp LICENSE "${LICENSE_DIR}/libbpf-${LIBBPF_VERSION}"

LIBBPF_OUTPUT="${OUTPUT_DIR}/libbpf"
mkdir -p "${LIBBPF_OUTPUT}"

make BUILD_STATIC_ONLY=y OBJDIR="${LIBBPF_OUTPUT}" "LDFLAGS=-Wl,-Bstatic" "CFLAGS=-fPIC ${EXTRA_CFLAGS_DEBUG:-}" \
     ${NPROCS:+-j ${NPROCS}} -C src install install_uapi_headers
