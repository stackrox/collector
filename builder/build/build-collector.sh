#!/usr/bin/env bash
set -e

if [ -f /etc/redhat-release ]; then
  source "/opt/rh/devtoolset-6/enable"
fi

set -ux
ldconfig -v
cd /build-output
cmake -DCMAKE_BUILD_TYPE=Release /src
make -j ${NPROCS} all
strip --strip-unneeded ./collector
