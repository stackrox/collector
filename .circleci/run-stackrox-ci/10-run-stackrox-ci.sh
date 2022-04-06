#!/usr/bin/env bash
set -eox pipefail

BRANCH=$1
BUILD_NUM=$2

comment_on_pr_with_ci_link() {
    local ci_url=$1

    wget --quiet https://github.com/joshdk/hub-comment/releases/download/0.1.0-rc6/hub-comment_linux_amd64
    sudo install hub-comment_linux_amd64 /usr/bin/hub-comment

    export CI_URL=$ci_url

    hub-comment -template-file "${CI_ROOT}/run-stackrox-ci/ci-url-comment-template.tpl"
}

git clone git@github.com:stackrox/stackrox.git
cd stackrox

git config user.email "$GITHUB_USER_NAME"
git config user.name "$GITHUB_USER_EMAIL"

stackrox_branch="${BRANCH}_COLLECTOR_CI"
if git branch | grep stackrox_branch; then
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
