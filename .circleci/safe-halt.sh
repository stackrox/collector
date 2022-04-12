#!/usr/bin/env bash

set -euo pipefail

for _ in {1..5}; do
    if command -v circleci; then
        circleci step halt
        exit 0
    fi
    sleep 5
done

echo >&2 "Failed to run 'circleci' command"
exit 1
