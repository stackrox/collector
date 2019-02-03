#!/usr/bin/env bash
PATH=/usr/lib/ccache:$PATH
set -e
cd
GOOGLETEST_REVISION=release-1.8.1
git clone https://github.com/google/googletest.git -b "$GOOGLETEST_REVISION" --depth 1
cd googletest
mkdir cmake-build
cd cmake-build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j 6
sudo make install
