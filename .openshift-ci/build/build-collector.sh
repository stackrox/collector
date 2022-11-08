#!/usr/bin/env bash
set -eo pipefail

CI_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"

# shellcheck source=SCRIPTDIR/../scripts/lib.sh
source "${CI_ROOT}/scripts/lib.sh"

export DISABLE_PROFILING="true"
export CMAKE_BUILD_DIR="$SRC_ROOT_DIR/cmake-build"

if is_in_PR_context; then
    export COLLECTOR_APPEND_CID=true
fi

make -C "$SRC_ROOT_DIR/collector" pre-build
"$SRC_ROOT_DIR/builder/build/build-collector.sh"
make -C "$SRC_ROOT_DIR/collector" post-build
