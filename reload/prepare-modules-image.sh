#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"

mod_ver="$1"
image_dir="$2"
gcp_bucket="$3"

if [[ -z "$mod_ver" || -z "$image_dir" || -z "$gcp_bucket" || $# -ne 3 ]]; then
    echo >&2 "Usage: $0 <module-version> <image-dir> <gcp-bucket>"
    exit 1
fi

container_dir="${image_dir}/${mod_ver}/container"
kernel_modules_dir="${container_dir}/kernel-modules"
mkdir -p "${kernel_modules_dir}"

gsutil -m rsync -r -x ".*\.unavail$" "${gcp_bucket}/${mod_ver}/" "${kernel_modules_dir}"

echo "${mod_ver}" > "${kernel_modules_dir}/MODULE_VERSION.txt"
rm -f "${kernel_modules_dir}"/.*.unavail

cp "${DIR}/../kernel-modules/container/"* "${container_dir}"

ls -al "${container_dir}"
ls -al "${kernel_modules_dir}"
