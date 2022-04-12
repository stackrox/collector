#!/usr/bin/env bash
set -eo pipefail

BRANCH=$1
BUILD_NUM=$2

comment_on_pr_with_ci_link() {
    local ci_url=$1

    export CI_URL=$ci_url

    hub-comment -template-file "${CI_ROOT}/run-stackrox-ci/ci-url-comment-template.tpl"
}

git config --global user.email "robot@stackrox.com"
git config --global user.name "RoxBot"

mkdir ~/.ssh || true
ssh-keyscan github.com >> ~/.ssh/known_hosts

git clone git@github.com:stackrox/stackrox.git
cd stackrox

stackrox_branch="collector_ci/${BRANCH}"
if git branch | grep -q "$stackrox_branch"; then
    git checkout "$stackrox_branch"
else
    git checkout -b "$stackrox_branch"
fi

echo "$COLLECTOR_VERSION" > COLLECTOR_VERSION
git add COLLECTOR_VERSION
git commit -m "Automatic commit ${BUILD_NUM}. Testing collector version ${COLLECTOR_VERSION}"

git push -f origin HEAD

DATA=$(jq -cn --arg branch "$stackrox_branch" '{
    "branch": $branch,
    "parameters": {
        "trigger_on_demand": false,
    }
}')

endpoint=https://circleci.com/api/v2/project/github/stackrox/stackrox/pipeline
response="$(curl -X POST --header 'Content-Type: application/json' \
        --header "Circle-Token: $CIRCLE_TOKEN_ROXBOT" "$endpoint" -d "$DATA")"

build_num="$(echo "$response" | jq '.number')"
ci_url="https://app.circleci.com/pipelines/github/stackrox/stackrox/$build_num"

echo "You can find the StackRox CircleCI results here $ci_url"

comment_on_pr_with_ci_link "$ci_url"
