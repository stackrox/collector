#!/usr/bin/env bash
set -eo pipefail

make -C "${SOURCE_ROOT}" unittest "USE_VALGRIND=$VALGRIND_UNIT_TESTS" "USE_HELGRIND=$HELGRIND_UNIT_TESTS" 2>&1 | tee "$UNITTEST_OUTPUT_FILE"
"${CI_ROOT}/check-file-for-valgrind-errors.sh" "$UNITTEST_OUTPUT_FILE" "$VALGRIND_UNIT_TESTS" "$HELGRIND_UNIT_TESTS"
