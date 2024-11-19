#!/usr/bin/env bash

set -e

cd third_party/bpftool

# Replace libbpf with our submodule
rm -rf libbpf/
ln -s ../libbpf libbpf

mkdir src/build
make V=1 -C src ${NPROCS:+-j ${NPROCS}} all install
