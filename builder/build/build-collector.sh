#!/usr/bin/env bash
set -e

set -ux
ldconfig -v
cd /tmp/cmake-build
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
echo "CMAKE_BUILD_TYPE= $CMAKE_BUILD_TYPE"
cmake -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE /src
make -j "${NPROCS:-2}" all
if [ $CMAKE_BUILD_TYPE = "Release" ]; then
  echo "Strip unneeded"
  strip --strip-unneeded \
      ./collector \
      ./EXCLUDE_FROM_DEFAULT_BUILD/userspace/libsinsp/libsinsp-wrapper.so
fi
cp -r /THIRD_PARTY_NOTICES .
