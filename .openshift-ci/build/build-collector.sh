#!/usr/bin/env bash
set -eo pipefail

export SRC_ROOT_DIR=$1
export DISABLE_PROFILING="true"
export CMAKE_BUILD_DIR="$SRC_ROOT_DIR/cmake-build"

make -C $SRC_ROOT_DIR/collector pre-build
/build-collector.sh
make -C $SRC_ROOT_DIR/collector post-build
