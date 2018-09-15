#!/usr/bin/env bash

set -e

git clone https://github.com/open-source-parsers/jsoncpp.git -b "$JSONCPP_REVISION" --depth 1
cd jsoncpp
mkdir cmake-build
cd cmake-build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j 6
make install
