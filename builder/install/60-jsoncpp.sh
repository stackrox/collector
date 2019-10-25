#!/usr/bin/env bash

set -e

git clone https://github.com/open-source-parsers/jsoncpp.git -b "$JSONCPP_REVISION" --depth 1
cd jsoncpp
mkdir cmake-build
cd cmake-build
cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-fPIC ..
make -j ${NPROCS}
make install
