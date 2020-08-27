#!/usr/bin/env bash
set -e

set -ux
ldconfig -v

cd /src/cmake-build
cmake -DCMAKE_BUILD_TYPE=Release /src

make -j 6

strip --strip-unneeded \
    ./collector
    ./EXCLUDE_FROM_DEFAULT_BUILD/userspace/libsinsp/libsinsp-wrapper.so
