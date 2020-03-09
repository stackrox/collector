#!/usr/bin/env bash

set -e

git clone -b "$GRPC_REVISION" --depth 1 https://github.com/grpc/grpc
cd grpc
git submodule update --init
make -j "${NPROCS:-2}" CXXFLAGS="-Wno-error=class-memaccess -Wno-ignored-qualifiers -Wno-stringop-truncation -Wno-cast-function-type" grpc_cpp_plugin static_cxx static_c
make prefix=/usr/local install
