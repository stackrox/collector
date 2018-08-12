#! /bin/sh
set -eux

mkdir -p /sysdig-src/build
cd /sysdig-src/build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_DRIVER=OFF ..
make
