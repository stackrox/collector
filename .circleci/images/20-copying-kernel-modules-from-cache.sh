#!/usr/bin/env bash
set -eo pipefail

cp -n "${WORKSPACE_ROOT}/ko-build/cached-probes/${MODULE_VERSION}"/* \
  "${SOURCE_ROOT}/kernel-modules/container/kernel-modules" || true
rm -f "${SOURCE_ROOT}/kernel-modules/container/kernel-modules/".*.unavail || true

ls "${SOURCE_ROOT}/kernel-modules/container/kernel-modules"
