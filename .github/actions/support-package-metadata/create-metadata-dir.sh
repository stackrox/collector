#!/usr/bin/env bash

set -euo pipefail

for version_dir in /tmp/support-packages/metadata/collector-versions/*; do
    version="$(basename "$version_dir")"
    git checkout "${version}"

    module_version_file="${GITHUB_WORKSPACE}/kernel-modules/MODULE_VERSION"
    if [[ -f "${module_version_file}" ]]; then
        cp "${module_version_file}" "${version_dir}/MODULE_VERSION"
    fi
done

git checkout "${BRANCH_NAME}"
