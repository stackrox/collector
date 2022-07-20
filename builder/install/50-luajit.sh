#!/usr/bin/env bash

set -e

if [ "$(arch)" = "x86_64" ]; then
  cd third_party/luajit
  cp COPYRIGHT "${LICENSE_DIR}/LuaJIT-${LUAJIT_VERSION}"
else
  cd third_party
  git clone https://github.com/openresty/luajit2.git
  cd luajit2
  git checkout v${LUAJIT_VERSION}
fi

make ${NPROCS:+-j ${NPROCS}} install PREFIX=/usr/local CFLAGS=-fPIC
