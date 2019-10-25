#!/usr/bin/env bash

set -e

wget "http://luajit.org/download/LuaJIT-${LUAJIT_VERSION}.tar.gz"
tar -zxf "LuaJIT-${LUAJIT_VERSION}.tar.gz"
cd "LuaJIT-${LUAJIT_VERSION}"
make -j ${NPROCS} install PREFIX=/usr/local CFLAGS=-fPIC
