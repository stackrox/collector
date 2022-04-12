#!/usr/bin/env bash
set -eo pipefail

source_root=$1
collector_modules_bucket=$2

for collector_dir in "/tmp/reload/collectors"/*; do
    cat "${collector_dir}/module-version"
done | sort | uniq > /tmp/reload/module-versions

while IFS='' read -r mod_ver || [[ -n "$mod_ver" ]]; do
    container_build_dir="${WORKSPACE_ROOT}/images/${mod_ver}/container"
    kernel_modules_dir="${container_build_dir}/kernel-modules"
    mkdir -p "${kernel_modules_dir}"
    echo "${mod_ver}" > "${kernel_modules_dir}/MODULE_VERSION.txt"
    # Sync kernel objects from GCS
    gsutil -m rsync -r -x ".*\.unavail$" \
        "${collector_modules_bucket}/${mod_ver}/" "${kernel_modules_dir}" | cat
    # Copy Dockerfile and build scripts into build directory
    cp "${source_root}/kernel-modules/container/"* "${container_build_dir}"
done < /tmp/reload/module-versions
