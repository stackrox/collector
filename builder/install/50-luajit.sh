#!/usr/bin/env bash

set -e

if [ -n "${WITH_RHEL8_RPMS}" ]; then
    cd luajit
else
    wget "http://luajit.org/download/LuaJIT-${LUAJIT_VERSION}.tar.gz"
    tar -zxf "LuaJIT-${LUAJIT_VERSION}.tar.gz"
    cd "LuaJIT-${LUAJIT_VERSION}"
fi
cp COPYRIGHT "${LICENSE_DIR}/LuaJIT-${LUAJIT_VERSION}"
make -j "${NPROCS:-2}" install PREFIX=/usr/local CFLAGS=-fPIC
