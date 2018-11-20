#! /bin/sh
set -eux

ldconfig -v
cd /build-output
cmake -DCMAKE_BUILD_TYPE=Release /src
make -j6 all
strip --strip-unneeded ./collector
./runUnitTests
