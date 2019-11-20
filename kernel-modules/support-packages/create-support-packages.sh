#!/usr/bin/env bash

set -euo pipefail

md_dir="$1"
output_dir="$2"

[[ -d "$md_dir" ]] || die "Metadata directory ${md_dir} does not exist."

mkdir -p "$output_dir" || die "Failed to create output directory ${output_dir}."

for mod_ver_dir in "${md_dir}/module-versions"/*; do
    mod_ver="$(basename "$mod_ver")"

    package_root="$(mktemp -d)"
    probe_dir="${package_root}/kernel-modules/${mod_ver}"
    mkdir -p "$probe_dir"
    {
        gsutil ls "${COLLECTOR_MODULES_BUCKET}/${mod_ver}/*.gz" | sed -E 's@^([^/]*/)*@@g'
        cat "${mod_ver_dir}/COMMON_INVENTORY"
    } | sort | uniq -u | awk -v PREFIX="${COLLECTOR_MODULES_BUCKET}/${mod_ver}" '{print PREFIX "/" "$1}' \
    | gsutil -m cp -I "$probe_dir"

    package_output_dir="${output_dir}/${mod_ver}"
    mkdir -p "$package_output_dir"
    filename="support-pkg-${mod_ver::6}-$(date '+%Y%m%d%H%M%S').zip"

    ( cd "$package_root" ; zip -r "${package_output_dir}/${filename}" . )
done

