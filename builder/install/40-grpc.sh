#!/usr/bin/env bash

set -e

git clone -b $GRPC_REVISION --depth 1 https://github.com/grpc/grpc
cd grpc
git submodule update --init
make -j 6
make prefix=/usr/local install
