#! /bin/sh
set -eux

ldconfig -v
mkdir -p /src/cmake-build
cd /src/cmake-build
cmake ..
make all
./runUnitTests
