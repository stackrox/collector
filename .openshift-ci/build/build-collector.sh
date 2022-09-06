#!/usr/bin/env bash
set -eo pipefail

export DISABLE_PROFILING="true"
export CMAKE_BUILD_DIR="$SRC_ROOT_DIR/cmake-build"
export COLLECTOR_APPEND_CID=true

# For all PRs build in debug mode, unless requested otherwise via labels.
if is_in_PR_context; then
    if ! pr_has_label "release-build-type"; then
        export CMAKE_BUILD_TYPE="Debug"
    fi
fi

make -C "$SRC_ROOT_DIR/collector" pre-build
"$SRC_ROOT_DIR/builder/build/build-collector.sh"
make -C "$SRC_ROOT_DIR/collector" post-build
