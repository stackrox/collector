#!/usr/bin/env bash
set -eo pipefail

for i in "${WORKSPACE_ROOT}/ko-build/module-versions"/*/; do
  version="$(basename "$i")"
  [[ "$version" != "$MODULE_VERSION" ]] || continue
  if [[ -f pr-metadata/labels/no-cache ]] ; then
      echo >~/kobuild-tmp/existing-modules-"${version}"
      echo "Skipping GCS cache for version: ${version}"
      continue
  fi
  ( gsutil ls "${COLLECTOR_MODULES_BUCKET}/${version}/" || true)  \
    | "${SOURCE_ROOT}/kernel-modules/build/extract-kernel-versions-from-module-files" \
      >~/kobuild-tmp/existing-modules-"${version}"

  echo "Existing modules for version: ${version}"
  cat ~/kobuild-tmp/existing-modules-"${version}"
  echo
done
