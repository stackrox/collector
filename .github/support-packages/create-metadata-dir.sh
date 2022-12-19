#!/usr/bin/env bash

set -euo pipefail

for version_dir in /tmp/support-packages/metadata/collector-versions/*; do
    version="$(basename "$version_dir")"
    git checkout "${version}"

    cp "${GITHUB_WORKSPACE}/kernel-modules/MODULE_VERSION" "${version_dir}/MODULE_VERSION"
done

git checkout "${BRANCH_NAME}"
