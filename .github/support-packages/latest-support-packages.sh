#!/usr/bin/env bash

set -euo pipefail

METADATA_PATH=$1
OUTPUT_PATH=$2
GCP_BUCKET=$3

for arch in x86_64 s390x ppc64le; do
    for version_path in "${METADATA_PATH}/module-versions"/*; do
        version="$(basename "${version_path}")"
        output_dir="${OUTPUT_PATH}/${arch}/${version}"

        mkdir -p "${output_dir}"
        if ! gsutil cp "gs://${GCP_BUCKET}/${arch}/${version}/latest" "${output_dir}/latest"; then
            continue
        fi

        latest_pkg="$(cat "${output_dir}/latest")"

        touch "${output_dir}/${latest_pkg}"
        touch "${output_dir}/${latest_pkg}.sha256"
        touch "${output_dir}/support-pkg-${version}-latest.zip"
        touch "${output_dir}/support-pkg-${version}-latest.zip.sha256"
    done
done
