#!/usr/bin/env bash

set -e

git clone https://github.com/google/googletest.git -b "$GOOGLETEST_REVISION" --depth 1
cd googletest
cp LICENSE "${LICENSE_DIR}/googletest-${GOOGLETEST_REVISION}"
mkdir cmake-build
cd cmake-build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j "${NPROCS:-2}"
make install
