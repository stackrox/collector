#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"

mod_ver="$1"
cache_root="$2"
gcp_bucket="$3"

if [[ -z "$mod_ver" || -z "$cache_root" || -z "$gcp_bucket" || $# -ne 3 ]]; then
    echo >&2 "Usage: $0 <module-version> <kernel-obj-cache-root> <gcp-bucket>"
    exit 1
fi

mkdir -p "${DIR}/images/${mod_ver}" "${cache_root}/${mod_ver}"

gsutil -m rsync -r -x ".*\.unavail$" "${gcp_bucket}/${mod_ver}/" "${cache_root}/${mod_ver}" | cat
echo "${mod_ver}" > "${cache_root}/${mod_ver}/MODULE_VERSION.txt"
rm -f "${cache_root}/${mod_ver}"/.*.unavail

mkdir -p "${DIR}/images/${mod_ver}/container"
tar -czf "${DIR}/images/${mod_ver}/container/${mod_ver}.tar.gz" -C "${cache_root}/${mod_ver}" .

cp "${DIR}/../kernel-modules/container/Dockerfile" "${DIR}/images/${mod_ver}/container/"
