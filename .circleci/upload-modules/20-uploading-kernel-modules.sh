#!/usr/bin/env bash
set -eo pipefail

TAG=$1
BRANCH=$2
BUILD_NUM=$3

target="${COLLECTOR_MODULES_BUCKET}"
if [[ "$BRANCH" != "master" && -z "$TAG" ]]; then
  target="gs://stackrox-collector-modules-staging/pr-builds/${BRANCH}/${BUILD_NUM}"
fi

shopt -s nullglob
shopt -s dotglob
for probes_dir in "${WORKSPACE_ROOT}/ko-build/built-probes"/*; do
  files=("${probes_dir}"/*.{gz,unavail})
  [[ "${#files[@]}" -gt 0 ]] || continue
  printf '%s\n' "${files[@]}" | gsutil -m cp -n -I "${target}/$(basename "$probes_dir")/"
done

if [[ "$BRANCH" == "master" || -n "$TAG" ]]; then
  # On PR/master builds, additionally upload modules from cache
  for probes_dir in "${WORKSPACE_ROOT}/ko-build/cached-probes"/*; do
    files=("${probes_dir}"/*.{gz,unavail})
    [[ "${#files[@]}" -gt 0 ]] || continue
    printf '%s\n' "${files[@]}" | gsutil -m cp -n -I "${target}/$(basename "$probes_dir")/"
  done
fi
