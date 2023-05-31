#!/usr/bin/env bash

set -e

cd third_party/gperftools

./autogen.sh
./configure
make
make install
