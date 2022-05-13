#!/usr/bin/env bash
set -exuo pipefail

BRANCH=$1

BUCKET="gs://stackrox-collector-modules-staging/cpaas/${BRANCH}"
MODULE_VERSION="$(cat "${SOURCE_ROOT}/kernel-modules/MODULE_VERSION")"

mkdir -p "${SOURCE_ROOT}/kernel-modules/container/kernel-modules"
gsutil -m cp -n \
    "${BUCKET}/${MODULE_VERSION}/*.gz" \
    "${SOURCE_ROOT}/kernel-modules/container/kernel-modules"
