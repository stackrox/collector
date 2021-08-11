#!/bin/bash
set -e

# This script takes a collector repo, collector version, and a GCS bucket and prints
# the probes from either that are not found in both sources and/or
# do not have the same hash.

die() {
    echo >&2 "$@"
    exit 1
}

image_repo="$1"
collector_version="$2"
gcp_bucket="$3"
output_dir="$4"

[[ -n "$image_repo" && -n "$collector_version" && -n "$gcp_bucket" && -n "$output_dir" ]] || \
    die "Usage: $0 <image-repo> <collector-version> <collector-module-gcp-bucket> <output directory>"
[[ -d "$output_dir" ]] || \
    die "Output directory ${output_dir} does not exist or is not a directory"

result_dir="${output_dir}/$(basename "${image_repo}"):${collector_version}"
mkdir -p "${result_dir}"

image="${image_repo}:${collector_version}-latest"

inspect_out="${result_dir}/image-probes"
docker run -i --rm --entrypoint /bin/sh "${image}" /dev/stdin >"${inspect_out}" <<EOF
set -e
cat /kernel-modules/MODULE_VERSION.txt
find /kernel-modules -name '*.gz' -type f | xargs md5sum 2>/dev/null | \
sed "s/\([[:alnum:]]\+\).*\(collector-.*\.gz\)/\2 \1/"
EOF

module_version="$(head -n 1 "${inspect_out}")"
echo "${module_version}" > "${result_dir}/module-version"

[[ $(gsutil ls "${gcp_bucket}/${module_version}/*.gz" 2> /dev/null) ]] || exit

{
    while IFS='' read -r line || [[ -n "$line" ]]; do
        [[ -n "$line" ]] || continue
        basename "$line"
    done < <(tail -n +2 "$inspect_out")

    gsutil hash -h -m "${gcp_bucket}/${module_version}/*.gz" | \
        tee "${result_dir}/gsutil-output" | \
        paste -d " " - - - | \
        sed "s/.*\(collector-.*.gz\).*Hash (md5)\:[[:space:]]*\([[:alnum:]]\+\).*/\1 \2/" | \
        tee "${result_dir}/gcp-bucket-probes"

} | sort | uniq -u | awk -F' ' '{print $1}' | sort | uniq \
    > "${result_dir}/missing-probes"
