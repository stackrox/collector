#!/usr/bin/env bash

set -eu

source builder/install/cmake.sh

cd third_party/googletest
cp LICENSE "${LICENSE_DIR}/googletest-${GOOGLETEST_REVISION}"

cmake_wrap "googletest" "${OUTPUT_DIR}"
