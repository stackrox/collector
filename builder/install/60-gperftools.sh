#!/usr/bin/env bash

set -e

git clone https://github.com/gperftools/gperftools.git -b "gperftools-${GPERFTOOLS_VERSION}" --depth 1
cd gperftools
cp COPYING "${LICENSE_DIR}/gperftools-${GPERFTOOLS_VERSION}"
./autogen.sh && ./configure && make -j "$(getconf _NPROCESSORS_ONLN)" && make install
