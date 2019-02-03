#!/usr/bin/env bash
PATH=/usr/lib/ccache:$PATH
set -e

cd
JSONCPP_REVISION=1.8.4
git clone https://github.com/open-source-parsers/jsoncpp.git -b "$JSONCPP_REVISION" --depth 1
cd jsoncpp
mkdir cmake-build
cd cmake-build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j 6
sudo make install
