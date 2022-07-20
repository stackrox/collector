#!/usr/bin/env bash

set -e

cd third_party/luajit

cp COPYRIGHT "${LICENSE_DIR}/LuaJIT-${LUAJIT_VERSION}"

make ${NPROCS:+-j ${NPROCS}} install PREFIX=/usr/local CFLAGS=-fPIC
