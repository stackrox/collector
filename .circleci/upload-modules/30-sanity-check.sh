#!/usr/bin/env bash
set -eo pipefail

TAG=$1
BRANCH=$2

if [[ "$BRANCH" != "master" && -z "$TAG" ]]; then
  exit 0
fi
gsutil ls "${COLLECTOR_MODULES_BUCKET}/${MODULE_VERSION}/"
