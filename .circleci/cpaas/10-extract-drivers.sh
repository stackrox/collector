#!/usr/bin/env bash

set -euo pipefail

TAG=$1
BRANCH=$2

mkdir -p "${WORKSPACE_ROOT}/cpaas/kernel-modules/"

docker login \
    --username "${REDHAT_USERNAME}" \
    --password "${REDHAT_PASSWORD}" \
    brew.registry.redhat.io

docker run --pull always --rm \
    -v "${WORKSPACE_ROOT}/cpaas/kernel-modules/":/output \
    brew.registry.redhat.io/rh-osbs/rhacs-collector-drivers-rhel8:rhacs-0.1-rhel-8-containers-candidate-59258-20220510095241 \
    "cp -r /kernel-modules/* /output/"

ls -la "${WORKSPACE_ROOT}/cpaas/kernel-modules"/**

# Create the metadata directories
for version in "${WORKSPACE_ROOT}/cpaas/kernel-modules"/*/; do
    md_version="$(basename "$version")"
    mkdir -p "/tmp/cpaas-support-packages/metadata/module-versions/$md_version"
done

# sync modules
target="${COLLECTOR_MODULES_BUCKET}/cpaas/"
if [[ "$BRANCH" != "master" && -z "$TAG" ]]; then
    target="gs://stackrox-collector-modules-staging/cpaas/${BRANCH}"
fi

shopt -s nullglob
shopt -s dotglob
for driver_dir in "${WORKSPACE_ROOT}/cpaas/kernel-modules"/*; do
    files=("${driver_dir}"/*.{gz,unavail})
    [[ "${#files[@]}" -gt 0 ]] || continue
    printf '%s\n' "${files[@]}" | gsutil -m cp -n -I "${target}/$(basename "$driver_dir")/"
done
