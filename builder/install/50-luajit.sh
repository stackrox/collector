#!/usr/bin/env bash

set -e

cd third_party/luajit2

cp COPYRIGHT "${LICENSE_DIR}/luajit2-${LUAJIT2_VERSION}"

make ${NPROCS:+-j ${NPROCS}} install PREFIX=/usr/local CFLAGS=-fPIC
