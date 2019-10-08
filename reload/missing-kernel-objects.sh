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

inspect_out="$(mktemp)"
docker run -i --rm --entrypoint /bin/sh "${image}" /dev/stdin >"${inspect_out}" <<EOF
set -e
cat /kernel-modules/MODULE_VERSION.txt
find /kernel-modules -name '*.gz' -type f
EOF

version="$(head -n 1 ${inspect_out})"

{
    while IFS='' read -r line || [[ -n "$line" ]]; do
        [[ -n "$line" ]] || continue
        basename "$line"
    done < <(tail -n +2 "$inspect_out")

    gsutil ls "${gcp_bucket}/${version}/*.gz" | sed -e 's@^.*/@@g'
} | sort | uniq -u
