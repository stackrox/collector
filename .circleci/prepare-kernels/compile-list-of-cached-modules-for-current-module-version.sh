#!/usr/bin/env bash
set -eo pipefail

( ls -a "${WORKSPACE_ROOT}/ko-build/cached-probes/${MODULE_VERSION}"/ || true ) \
  | "${SOURCE_ROOT}/kernel-modules/build/extract-kernel-versions-from-module-files" \
  >~/kobuild-tmp/existing-modules-"${MODULE_VERSION}"
