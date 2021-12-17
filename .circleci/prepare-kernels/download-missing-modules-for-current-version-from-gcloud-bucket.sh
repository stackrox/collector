#!/usr/bin/env bash
set -eo pipefail

mkdir -p "${WORKSPACE_ROOT}/ko-build/cached-probes/${MODULE_VERSION}"
[[ ! -f pr-metadata/labels/no-cache ]] || exit 0
gsutil -m rsync -r \
  "${COLLECTOR_MODULES_BUCKET}/${MODULE_VERSION}/" \
  "${WORKSPACE_ROOT}/ko-build/cached-probes/${MODULE_VERSION}/" \
  || true
