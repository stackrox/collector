#!/usr/bin/env bash
set -eo pipefail

export DISABLE_PROFILING="true"
export CMAKE_BUILD_DIR="$SRC_ROOT_DIR/cmake-build"
export COLLECTOR_APPEND_CID=true

make -C "$SRC_ROOT_DIR/collector" pre-build
"$SRC_ROOT_DIR/builder/build-collector.sh"
make -C "$SRC_ROOT_DIR/collector" post-build
