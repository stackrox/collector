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
    branch_exists="true"
else
    git checkout -b "$stackrox_branch"
    branch_exists="false"
fi

echo "$COLLECTOR_VERSION" > COLLECTOR_VERSION
git add COLLECTOR_VERSION
git commit -m "Automatic commit ${BUILD_NUM}. Testing collector version ${COLLECTOR_VERSION}"

if [ "$branch_exists" = "false" ]; then
    git clone git@github.com:stackrox/rox-ci-image.git
    cd rox-ci-image
    git checkout 67b3694
    cd ..
    repo=stackrox
    pr_title="$stackrox_branch"
    pr_description_body="test"
    rox-ci-image/images/static-contents/scripts/create_update_pr.sh "$stackrox_branch" "$repo" "$pr_title" "$pr_description_body"
fi

git push -f origin HEAD

stackrox_branch_encoded="${stackrox_branch/\//%2F}"
pr_url="https://app.circleci.com/pipelines/github/stackrox/stackrox?branch=${stackrox_branch_encoded}&filter=all"

echo "You can find the CircleCI results here $pr_url"
