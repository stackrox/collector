#!/usr/bin/env bash

set -e

git clone https://github.com/open-source-parsers/jsoncpp.git -b "$JSONCPP_REVISION" --depth 1
cd jsoncpp
cp LICENSE "${LICENSE_DIR}/jsoncpp-${JSONCPP_REVISION}"
mkdir cmake-build
cd cmake-build
cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-fPIC ..
make -j "${NPROCS:-2}"
make install
