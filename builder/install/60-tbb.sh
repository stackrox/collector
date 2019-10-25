#!/usr/bin/env bash

set -e

git clone --branch "$TBB_VERSION" --depth 1 https://github.com/intel/tbb
cd tbb
make -j ${NPROCS} tbb_build_dir=./build tbb_build_prefix=lib extra_inc=big_iron.inc 
mkdir -p /usr/local/include
cp -r ./include/tbb /usr/local/include/
cp ./build/lib_release/libtbb.a /usr/local/lib/
