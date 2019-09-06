#!/bin/bash
set -e

image="$1"
gcp_bucket="${COLLECTOR_MODULES_BUCKET:-gs://collector-modules/612dd2ee06b660e728292de9393e18c81a88f347ec52a39207c5166b5302b656}"

dir="$(mktemp -d)"
name="collector-$RANDOM"

docker create --name="${name}" "${image}" > /dev/null 2>&1
docker cp "${name}:/kernel-modules/" "$dir"

version="$(cat "$dir/kernel-modules/MODULE_VERSION.txt")"

comm -1 -3 \
   <( find "${dir}/kernel-modules/" -name "*.gz" | awk -F '/' -v version="$version" '{print version "/" $NF}' | sort ) \
   <( gsutil ls "${gcp_bucket}/${version}" | grep ".*.gz$" | awk -F '/' '{print $(NF-1) "/" $NF}' | sort )

rm -rf "$dir"
docker rm -fv "${name}" > /dev/null 2>&1
