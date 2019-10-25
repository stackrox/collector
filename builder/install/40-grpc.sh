#!/usr/bin/env bash

set -e

git clone -b "$GRPC_REVISION" --depth 1 https://github.com/grpc/grpc
cd grpc
git submodule update --init
make -j ${NPROCS} grpc_cpp_plugin static_cxx static_c
make prefix=/usr/local install
