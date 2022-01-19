#!/usr/bin/env bash
set -eo pipefail

[[ -d "/tmp/cache/kernel-modules/${MODULE_VERSION}/" ]] || exit 0
mkdir -p "${WORKSPACE_ROOT}/ko-build/cached-probes/${MODULE_VERSION}"
mv -v \
    "/tmp/cache/kernel-modules/${MODULE_VERSION}"/* \
    "${WORKSPACE_ROOT}/ko-build/cached-probes/${MODULE_VERSION}/" \
    || true
