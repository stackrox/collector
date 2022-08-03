#!/usr/bin/env bash

set -e

if [ "$(arch)" = "x86_64" ]; then
    cd third_party/luajit
    cp COPYRIGHT "${LICENSE_DIR}/LuaJIT-${LUAJIT_VERSION}"
else
    cd third_party/luajit2
    cp COPYRIGHT "${LICENSE_DIR}/LuaJIT-${LUAJIT2_VERSION}"
fi

make ${NPROCS:+-j ${NPROCS}} install PREFIX=/usr/local CFLAGS=-fPIC
