#!/usr/bin/env bash
set -e

set -ux
ldconfig -v
cd /tmp/cmake-build
cmake -DCMAKE_BUILD_TYPE=Debug /src
make -j "${NPROCS:-2}" all
#strip --strip-unneeded \
#    ./collector \
#    ./EXCLUDE_FROM_DEFAULT_BUILD/userspace/libsinsp/libsinsp-wrapper.so
cp -r /THIRD_PARTY_NOTICES .
