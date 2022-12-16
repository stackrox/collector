#!/usr/bin/env bash

set -euo pipefail

for version_dir in /tmp/support-packages/metadata/collector-versions/*; do
    version="$(basename "$version_dir")"
    git checkout "${version}"

    driver_version="$(cat "${GITHUB_WORKSPACE}/kernel-modules/MODULE_VERSION")"
    echo "${driver_version}" > "${version_dir}/MODULE_VERSION"
done
