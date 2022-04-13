#!/usr/bin/env bash

set -euo pipefail

# This script checks the 'circleci-agent' command exists before attempting to
# call it. We were having failures in CircleCI where the 'circleci' command was
# not found, so this script attempts to mitigate this problem by:
# - Checking the script exists before calling it.
# - Retry up to 5 times every 5 seconds if the first call fails.
#
# The 'circleci' command was also replace by ' circleci-agent', since this last
# command is actually in charge of handling 'circleci step' calls.

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
