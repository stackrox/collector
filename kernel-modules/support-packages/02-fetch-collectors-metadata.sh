#!/usr/bin/env bash

set -euo pipefail

die() {
    echo >&2 "$@"
    exit 1
}

MD_DIR="$1"
SOURCE_ROOT="$2"

[[ -n "$MD_DIR" ]] || die "Usage: $0 <metadata directory>"
[[ -d "$MD_DIR" ]] || die "Metadata directory $MD_DIR does not exist or is not a directory."

for version_dir in "${MD_DIR}/collector-versions"/*; do
    [[ -d "$version_dir" ]] || continue
    version="$(basename "$version_dir")"

    if [[ "$version" != master ]]; then
        collector_image="quay.io/rhacs-eng/collector:${version}"
        docker pull "$collector_image"
        tmp_output="$(mktemp)"

        docker run --rm --entrypoint /bin/bash "$collector_image" -c '
            cd /kernel-modules &&
            head -n 1 MODULE_VERSION.txt &&
            ls *.gz
        ' > "$tmp_output"

        head -n 1 "$tmp_output" > "${version_dir}/MODULE_VERSION"
        tail -n +2 "$tmp_output" > "${version_dir}/INVENTORY"
        rm -f "$tmp_output" || true
        docker image rm "$collector_image"
    fi
done

master_dir="${MD_DIR}/collector-versions/master"
mkdir -p "${master_dir}"
cd "${SOURCE_ROOT}"
git checkout master -- "kernel-modules/MODULE_VERSION"
cp "kernel-modules/MODULE_VERSION" "$master_dir"
