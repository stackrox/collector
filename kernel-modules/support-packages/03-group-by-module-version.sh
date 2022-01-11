#!/usr/bin/env bash

set -euo pipefail

die() {
    echo >&2 "$@"
    exit 1
}

MD_DIR="$1"

[[ -n "$MD_DIR" ]] || die "Usage: $0 <metadata directory>"
[[ -d "$MD_DIR" ]] || die "Metadata directory $MD_DIR does not exist or is not a directory."

for version_dir in "${MD_DIR}/collector-versions"/*; do
    [[ -d "$version_dir" ]] || continue
    version="$(basename "$version_dir")"

    module_version="$(< "${version_dir}/MODULE_VERSION")"

    mod_ver_dir="${MD_DIR}/module-versions/${module_version}"
    mkdir -p "$mod_ver_dir"

    tmpfile="$(mktemp)"
    (   
        cat "${version_dir}/ROX_VERSIONS"
        cat 2> /dev/null "${mod_ver_dir}/ROX_VERSIONS" || true
    ) | sort | uniq > "$tmpfile"
    mv "$tmpfile" "${mod_ver_dir}/ROX_VERSIONS"
done
