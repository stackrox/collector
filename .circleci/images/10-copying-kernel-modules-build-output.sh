#!/usr/bin/env bash
set -eo pipefail

mkdir -p "${SOURCE_ROOT}/kernel-modules/container/kernel-modules"
mv "${WORKSPACE_ROOT}/ko-build/built-probes/${MODULE_VERSION}/"* \
    "${SOURCE_ROOT}/kernel-modules/container/kernel-modules" || true
rm -f "${SOURCE_ROOT}/kernel-modules/container/kernel-modules/".*.unavail || true

ls "${SOURCE_ROOT}/kernel-modules/container/kernel-modules"
