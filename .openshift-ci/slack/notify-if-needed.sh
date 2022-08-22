#!/usr/bin/env bash
set -eo pipefail

export BRANCH="$(echo "$JOB_SPEC" | jq -r '.extra_refs[0].base_ref')"

# No notifications for feature branches
if [[ ! "$BRANCH" =~ ^(master|main|release)$ ]]; then
    echo "Suppress notifications: not on the main branch"
    exit 0
fi

# No notifications outside of the CI pipeline
if [[ -z "$CI" ]]; then
    echo "Suppress notifications: not on the CI pipeline"
    exit 0
fi

.openshift-ci/slack/notify.sh $@
