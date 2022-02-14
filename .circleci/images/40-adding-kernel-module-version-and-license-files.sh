#!/usr/bin/env bash
set -eo pipefail

mkdir -p "${SOURCE_ROOT}/kernel-modules/container/kernel-modules"
echo "$MODULE_VERSION" \
    >> "${SOURCE_ROOT}/kernel-modules/container/kernel-modules/MODULE_VERSION.txt"
cp "${SOURCE_ROOT}"/collector/LICENSE-kernel-modules.txt "${SOURCE_ROOT}"/collector/container/
