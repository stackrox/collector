#!/usr/bin/env bash
PATH=/usr/lib/ccache:$PATH
set -e

cd
GRPC_REVISION=v1.15.0
git clone -b $GRPC_REVISION --depth 1 https://github.com/grpc/grpc
cd grpc
git submodule update --init
make -j 6
sudo make prefix=/usr/local install
