#!/usr/bin/env bash
set -eox pipefail

comment_on_pr_with_ci_link() {
    local ci_url=$1

    wget --quiet https://github.com/joshdk/hub-comment/releases/download/0.1.0-rc6/hub-comment_linux_amd64
    sudo install hub-comment_linux_amd64 /usr/bin/hub-comment

    export CI_URL=$ci_url

    hub-comment -template-file "${CI_ROOT}/run-stackrox-ci/ci-url-comment-template.tpl"
}

DATA=$(jq -cn --arg collector_version "$COLLECTOR_VERSION" '{
    "branch": "master",
    "parameters": {
        "trigger_on_demand": true,
	"workflow_name": "build_all",
	"collector_version": $collector_version
    }
}')

endpoint=https://circleci.com/api/v2/project/github/stackrox/stackrox/pipeline
response="$(curl -X POST --header 'Content-Type: application/json' \
        --header "Circle-Token: $CIRCLE_TOKEN_ROXBOT" "$endpoint" -d "$DATA")"

build_num="$(echo "$response" | jq '.number')"
ci_url="https://app.circleci.com/pipelines/github/stackrox/stackrox/$build_num"

echo "You can find the StackRox CircleCI results here $ci_url"

comment_on_pr_with_ci_link "$ci_url"
