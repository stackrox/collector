#!/usr/bin/env bash
set -eo pipefail

cd "${WORKSPACE_ROOT}/ko-build/module-versions"
for version in *; do
    [[ -f "${SOURCE_ROOT}/kernel-modules/patches/${version}.patch" ]] || continue
    echo "Applying patch for module version ${version} ..."
    patch -p1 -d "$version" < "${SOURCE_ROOT}/kernel-modules/patches/${version}.patch"
done
