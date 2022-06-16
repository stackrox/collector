#!/usr/bin/env bash
set -eo pipefail

export SRC_ROOT_DIR=$1
export DISABLE_PROFILING="true"
export CMAKE_BUILD_DIR="$SRC_ROOT_DIR/cmake-build"

make -C /tmp/collector/collector pre-build
/build-collector.sh
make -C /tmp/collector/collector post-build
