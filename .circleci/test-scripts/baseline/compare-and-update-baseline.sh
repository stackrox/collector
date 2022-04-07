#!/usr/bin/env bash
set -euo pipefail

pip install --upgrade scipy google-cloud-storage

jq -s 'flatten' "${WORKSPACE_ROOT}"/*perf.json > "${WORKSPACE_ROOT}"/all-perf.json

export BASELINE="${CI_ROOT}/test-scripts/baseline/"
"${BASELINE}"/main.py --test "${WORKSPACE_ROOT}"/all-perf.json \
    | sort \
    | awk -f "${BASELINE}"/format.awk > "${WORKSPACE_ROOT}"/benchmark.md

# The baseline has to be updated only in the main branch.
# Take into account that CIRCLE_TAG maybe not set.
if [[ "$CIRCLE_BRANCH" == "master" || -n "${CIRCLE_TAG:-}" ]]; then
    "${BASELINE}"/main.py --update "${WORKSPACE_ROOT}"/all-perf.json
fi
