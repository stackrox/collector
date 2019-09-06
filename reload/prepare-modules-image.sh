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

gsutil -m rsync -r \
  "${gcp_bucket}/${mod_ver}/" "${cache_root}/${mod_ver}"

mkdir -p "${DIR}/images/${mod_ver}/kernel-modules"

rsync -a "${cache_root}/${mod_ver}"/*.gz "${DIR}/images/${mod_ver}/kernel-modules"

cd "${DIR}/images/${mod_ver}"
docker build \
  -t "stackrox-kernel-modules:${mod_ver}" \
  -f - \
  . < "${DIR}/../kernel-modules/container/Dockerfile"

