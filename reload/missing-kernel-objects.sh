#!/bin/bash
set -e

# This script takes a collector image and a GCS bucket and prints
# the probes from either that are not found in both sources and/or
# do not have the same hash.

if [[ ! $# -eq 2 ]] ; then
  echo "Usage: $0 <collector-image> gs://<collector-module-bucket-path>"
  exit 1
fi

image="$1"
gcp_bucket="$2"

inspect_out="$(mktemp)"
docker run -i --rm --entrypoint /bin/sh "${image}" /dev/stdin >"${inspect_out}" <<EOF
set -e
cat /kernel-modules/MODULE_VERSION.txt
find /kernel-modules -name '*.gz' -type f | xargs md5sum 2>/dev/null | \
sed "s/\([[:alnum:]]\+\).*\(collector-.*\.gz\)/\2 \1/"
EOF

version="$(head -n 1 "${inspect_out}")"

[[ $(gsutil ls "${gcp_bucket}/${version}/*.gz" 2> /dev/null) ]] || exit

{
    while IFS='' read -r line || [[ -n "$line" ]]; do
        [[ -n "$line" ]] || continue
        basename "$line"
    done < <(tail -n +2 "$inspect_out")

    gsutil hash -h "${gcp_bucket}/${version}/*.gz" | \
        paste -d " " - - - | \
        sed "s/.*\(collector-.*.gz\).*Hash (md5)\:[[:space:]]*\([[:alnum:]]\+\)/\1 \2/"

} | sort | uniq -u | awk -F' ' '{print $1}' | sort | uniq
