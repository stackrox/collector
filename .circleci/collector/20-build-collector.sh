#!/usr/bin/env bash
set -eo pipefail

extra_flags=(
    CMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE"
    USE_VALGRIND="$BUILD_USE_VALGRIND"
    ADDRESS_SANITIZER="$SANITIZER_TESTS"
    COLLECTOR_APPEND_CID="$COLLECTOR_APPEND_CID"
)
make -C "${SOURCE_ROOT}" collector "${extra_flags[@]}"
