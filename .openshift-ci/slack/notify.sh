#!/usr/bin/env bash
set -eo pipefail

CI_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
# shellcheck source=SCRIPTDIR=../scripts/lib.sh
source "${CI_ROOT}/scripts/lib.sh"

import_creds

WEBHOOK_URL=$SLACK_WEBHOOK_ONCALL
JOB_URL="https://prow.ci.openshift.org/view/gs/origin-ci-test/logs/${JOB_NAME}/${BUILD_ID}"
JOB_STEP=$1

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}")"  &> /dev/null && pwd)
BODY=$(cat "${SCRIPT_DIR}/template")

jq --null-input \
    --arg job_name "$JOB_NAME" \
    --arg job_url "$JOB_URL" \
    --arg job_step "$JOB_STEP" \
    --arg mentions "@collector-team" \
    "${BODY}" \
              | curl -XPOST -d @- \
        -H 'Content-Type: application/json' \
        "$WEBHOOK_URL"
