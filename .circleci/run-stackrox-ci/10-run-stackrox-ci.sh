#!/usr/bin/env bash
set -eox pipefail

BRANCH=$1
BUILD_NUM=$2

git clone git@github.com:stackrox/stackrox.git
cd stackrox

git config user.email "$GITHUB_USER_NAME"
git config user.name "$GITHUB_USER_EMAIL"

stackrox_branch="${BRANCH}_CI"
if git branch | grep stackrox_branch; then
    git checkout "$stackrox_branch"
else
    git checkout -b "$stackrox_branch"
fi

echo "$COLLECTOR_VERSION" > COLLECTOR_VERSION
git add COLLECTOR_VERSION
git commit -m "Automatic commit ${BUILD_NUM}. Testing collector version ${COLLECTOR_VERSION}"

git push -f origin HEAD

generate_data() {
    cat << EOF
    {
         "branch": "$stackrox_branch",
         "parameters": {
             "trigger_on_demand": false
         }
    }
EOF
}

curl -X POST --header 'Content-Type: application/json' \
        --header "Circle-Token: $CIRCLE_TOKEN_ROXBOT" https://circleci.com/api/v2/project/github/stackrox/stackrox/pipeline -d "$(generate_data)"

stackrox_branch_encoded="${stackrox_branch/\//%2F}"
pr_url="https://app.circleci.com/pipelines/github/stackrox/stackrox?branch=${stackrox_branch_encoded}&filter=all"

echo "You can find the CircleCI results here $pr_url"
