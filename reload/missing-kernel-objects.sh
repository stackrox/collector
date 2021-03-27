#!/bin/bash
set -e

# This script takes a collector image and a GCS bucket and prints
# the probes from either that are not found in both sources and/or
# do not have the same hash.

die() {
    echo >&2 "$@"
    exit 1
}

collector_version="$1"
gcp_bucket="$2"
output_dir="$3"

[[ -n "$collector_version" && -n "$gcp_bucket" && -n "$output_dir" ]] || \
    die "Usage: $0 <collector-version> <collector-module-gcp-bucket> <output directory>"
[[ -d "$output_dir" ]] || \
    die "Output directory ${output_dir} does not exist or is not a directory"

mkdir -p "${output_dir}/${collector_version}"

image="stackrox/collector:${collector_version}-latest"

inspect_out="${output_dir}/${collector_version}/image-probes"
docker run -i --rm --entrypoint /bin/sh "${image}" /dev/stdin >"${inspect_out}" <<EOF
set -e
cat /kernel-modules/MODULE_VERSION.txt
find /kernel-modules -name '*.gz' -type f | xargs md5sum 2>/dev/null | \
sed "s/\([[:alnum:]]\+\).*\(collector-.*\.gz\)/\2 \1/"
EOF

module_version="$(head -n 1 "${inspect_out}")"

[[ $(gsutil ls "${gcp_bucket}/${module_version}/*.gz" 2> /dev/null) ]] || exit

{
    while IFS='' read -r line || [[ -n "$line" ]]; do
        [[ -n "$line" ]] || continue
        basename "$line"
    done < <(tail -n +2 "$inspect_out")

    gsutil hash -h -m "${gcp_bucket}/${module_version}/*.gz" | \
        tee "${output_dir}/${collector_version}/gsutil-output" | \
        paste -d " " - - - | \
        tee "${output_dir}/${collector_version}/paste-output" | \
        sed "s/.*\(collector-.*.gz\).*Hash (md5)\:[[:space:]]*\([[:alnum:]]\+\).*/\1 \2/" | \
        tee "${output_dir}/${collector_version}/bucket-probes"

} | sort | uniq -u | awk -F' ' '{print $1}' | sort | uniq \
    > "${output_dir}/${collector_version}/missing-probes"
