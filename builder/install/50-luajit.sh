#!/usr/bin/env bash

set -e

cd third_party/luajit2
git checkout v${LUAJIT2_VERSION}
cp COPYRIGHT "${LICENSE_DIR}/LuaJIT-${LUAJIT2_VERSION}"

make ${NPROCS:+-j ${NPROCS}} install PREFIX=/usr/local CFLAGS=-fPIC
