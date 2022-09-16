#!/usr/bin/env bash
set -eo pipefail

ERROR_MSG=$1
ERROR_CODE=$2

export WORKDIR=/go/src/github.com/stackrox/collector

# shellcheck source=SCRIPTDIR/../drivers/scripts/lib.sh
source "$WORKDIR/.openshift-ci/drivers/scripts/lib.sh"

# No notifications for feature branches
if is_in_PR_context; then
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
