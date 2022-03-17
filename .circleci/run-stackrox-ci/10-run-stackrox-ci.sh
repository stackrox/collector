#!/usr/bin/env bash
set -eo pipefail

git clone git@github.com:stackrox/stackrox.git
cd stackrox

stackrox_branch="${BRANCH}_CI"
if git branch | grep stackrox_branch; then
    git checkout "$stackrox_branch"
else
    git checkout -b "$stackrox_branch"
fi

echo "$COLLECTOR_TAG" > COLLECTOR_VERSION
git add COLLECTOR_VERSION
git commit -m "Automatic commit ${BUILD_NUM}. Testing collector version ${COLLECTOR_TAG}"
git push -f origin HEAD
