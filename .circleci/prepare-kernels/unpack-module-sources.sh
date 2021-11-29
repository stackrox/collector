#!/usr/bin/env bash
set -eo pipefail

mkdir -p "${WORKSPACE_ROOT}/ko-build"
cd "${WORKSPACE_ROOT}/ko-build"

echo >build-tasks
mkdir build-output

for i in ~/kobuild-tmp/versions-src/*.tgz; do
  version="$(basename "$i" .tgz)"
  mkdir -p "module-versions/${version}"
  tar -C "module-versions/${version}" -xvzf "$i"
  mkdir -p "build-output/${MODULE_VERSION}"
done
