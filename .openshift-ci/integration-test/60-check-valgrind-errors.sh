#!/usr/bin/env bash
set -eo pipefail

SOURCE_ROOT=$1
CI_ROOT=$2

if [[ "$CHECK_VALGRIND_INTEGRATION_TESTS" != "true" ]]; then
    echo "Not using Valgrind. Not checking for Valgrind errors"
    exit 0
fi
echo "Using Valgrind"
log_dir="${SOURCE_ROOT}/integration-tests/container-logs"
for file in "$log_dir"/*-collector.log; do
    "${CI_ROOT}/check-file-for-valgrind-errors.sh" "$file" "$VALGRIND_INTEGRATION_TESTS" "$HELGRIND_INTEGRATION_TESTS"
done
