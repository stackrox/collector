#!/usr/bin/env bash

set -exuo pipefail

# shellcheck source=SCRIPTDIR/lib.sh
source /scripts/lib.sh

# This scripts checks and sets a number of varibales needed to patch driver
# files and split tasks needed to build them

export BRANCH
BRANCH="$(get_base_ref)"

export LEGACY_PROBES
export NO_CACHE

LEGACY_PROBES="false"
NO_CACHE=0

if ! is_in_PR_context; then
    LEGACY_PROBES="true"
    NO_CACHE=1
else
    if pr_has_label "build-legacy-probes"; then
        LEGACY_PROBES="true"
    fi

    if pr_has_label "no-cache"; then
        NO_CACHE=1
    fi
fi
