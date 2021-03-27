#!/usr/bin/env bash

DIR="$(cd "$(dirname "$0")" && pwd)"
set -e

collector_ver="$1"
mod_ver="$2"
cache_root="$3"
gcp_bucket="$4"

if [[ -z "$collector_ver" || -z "$mod_ver" || -z "$cache_root" || -z "$gcp_bucket" || $# -ne 4 ]]; then
  echo >&2 "$@"
  echo >&2 "Usage: $0 <collector-version> <module-version> <cache-root> <gcp-bucket>"
  exit 1
fi

released_latest_image="stackrox/collector:${collector_ver}-latest"

if [[ ! -d "${DIR}/images/${mod_ver}/container" ]]; then
  echo "Syncing kernel modules for ${mod_ver}"
  "${DIR}/prepare-modules-image.sh" "${mod_ver}" "${cache_root}" "${gcp_bucket}"
fi

build_args=(
  --build-arg module_version="${mod_ver}"
  --build-arg collector_version="${collector_ver}"
)

docker build \
  -t "collector.stackrox.io/collector:${collector_ver}-latest" \
  -t "${released_latest_image}" \
  -t "stackrox/collector:${collector_ver}-reload-latest" \
  "${build_args[@]}" \
  "${DIR}/images/${mod_ver}/container"
