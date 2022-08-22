#!/usr/bin/env bash
set -eo pipefail

ERROR_MSG=$1
ERROR_CODE=$2

# shellcheck disable=SC2155
export BRANCH="$(echo "$JOB_SPEC" | jq -r '.extra_refs[0].base_ref')"

# No notifications for feature branches
if [[ ! "$BRANCH" =~ ^(master|main|release)$ ]]; then
    echo "Suppress notifications: not on the main branch"
    exit "$ERROR_CODE"
fi

# No notifications outside of the CI pipeline
if [[ -z "$CI" ]]; then
    echo "Suppress notifications: not on the CI pipeline"
    exit "$ERROR_CODE"
fi

.openshift-ci/slack/notify.sh "$ERROR_MSG"

exit "$ERROR_CODE"
