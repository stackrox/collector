#!/usr/bin/env bash

set -euo pipefail

circleci_halt() {
    if command -v circleci-agent > /dev/null; then
        circleci-agent step halt
        exit 0
    fi
}

circleci_halt

for _ in {1..5}; do
    sleep 5
    circleci_halt
done

echo >&2 "Failed to run 'circleci-agent' command"
exit 1
