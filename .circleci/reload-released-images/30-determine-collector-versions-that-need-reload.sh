#!/usr/bin/env bash
set -eo pipefail

source_root=$1
reload_md_directory=$2
docker_repo=$3
collector_modules_bucket=$4

mkdir -p "${reload_md_directory}"
for collector_ver_file in "${WORKSPACE_ROOT}/ko-build/released-collectors"/*; do
    collector_ver="$(basename "${collector_ver_file}")"
    mod_ver="$(< "${collector_ver_file}")"
    echo "Missing object check for module version: ${mod_ver} and collector version: ${collector_ver}"
    released_latest_image="${docker_repo}/collector:${collector_ver}-latest"
    docker pull -q "${released_latest_image}"
    "${source_root}/reload/missing-kernel-objects.sh" \
        "${collector_ver}" "${collector_modules_bucket}" "${reload_md_directory}"
    missing_objs_file="${reload_md_directory}/${collector_ver}/missing-probes"
    if [[ ! -s "${missing_objs_file}" ]]; then
        echo "Image ${released_latest_image} contains up to date kernel objects"
        continue
    fi
    echo "Found $(wc -l < "${missing_objs_file}") missing or updated kernel objects."
done
