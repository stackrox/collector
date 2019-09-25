#!/bin/bash
set -e
# Usage: ./missing-kernel-objects.sh stackrox/collector:1.6.0-317-g903fb094c8 gs://collector-modules-bucket/

# This script is takes a collector image and prints a list of kernel modules and ebpf probes
# that are available in COLLECTOR_MODULES_BUCKET on GCP but that are not contained in the image.
# The list is printed in the form {module_version}/{kernel-object-name}.gz

if [[ ! $# -eq 2 ]] ; then
  echo "Usage: $0 <collector-image> <gs://collector-module-bucket-path>"
  exit 1
fi

image="$1"
gcp_bucket="$2"

version=$(docker run --rm --entrypoint cat "${image}" "/kernel-modules/MODULE_VERSION.txt")
kernel_modules=$(docker run --rm --entrypoint find "${image}" "/kernel-modules/" -name "*.gz" -type f -printf '%f\n')

# determine which kernel module and probes are missing from this image
comm -1 -3 \
   <( awk -v version="$version" '{print version "/" $1}' <(printf '%s\n' "$kernel_modules") | sort ) \
   <( gsutil ls "${gcp_bucket}/${version}" | grep ".*.gz$" | awk -F '/' '{print $(NF-1) "/" $NF}' | sort )
