#!/bin/bash
set -e

# This script takes a collector image and prints a list of kernel modules and
# ebpf probes that are available in the provided path on GCP but are not
# contained in the image, or differ from the image.

function print_gcp_modules_with_hash {
    local gcp_bucket=$1
    local version=$2
    gsutil hash -h "${gcp_bucket}/${version}/*.gz" | \
        paste -d " " - - - | \
        gsed "s/.*\(collector-.*.gz\).*Hash (md5)\:[[:space:]]*\([[:alnum:]]\+\)/\1 \2/" \
        2> /dev/null
}
function print_image_modules_with_hash {
    local inspect_out=$1
    while IFS='' read -r line || [[ -n "$line" ]]; do
        [[ -n "$line" ]] || continue
        basename "$line"
    done < <(tail -n +2 "$inspect_out")
}

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

image_modules="$(print_image_modules_with_hash "$inspect_out")"
gcp_modules="$(print_gcp_modules_with_hash "$gcp_bucket" "$version")"

[[ "$gcp_modules" ]] || exit

{
    echo "${image_modules}"
    echo "${image_modules}"
    echo "${gcp_modules}"

} | sort | uniq -u | awk -F' ' '{print $1}' | sort | uniq
