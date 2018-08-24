#!/usr/bin/env sh

git clone -b $GRPC_REVISION https://github.com/grpc/grpc \
    && cd grpc \
    && git submodule update --init \
    && make \
    && make prefix=/usr/local install
