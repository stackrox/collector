#! /bin/sh
set -eux

ldconfig -v
mkdir -p /src/cmake-build
cd /src/cmake-build
cmake -DCMAKE_BUILD_TYPE=Release ..
make all
strip --strip-unneeded ./collector
./runUnitTests
