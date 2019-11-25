#!/usr/bin/env bash
set -e

if [ -f /etc/redhat-release ]; then
  source "/opt/rh/devtoolset-6/enable"
fi

set -ux
ldconfig -v
cd /build-output
cmake -DCMAKE_BUILD_TYPE=Debug /src
make -j "${NPROCS:-2}" all
#strip --strip-unneeded ./collector
