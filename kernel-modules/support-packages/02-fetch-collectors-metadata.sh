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

    collector_image="collector.stackrox.io/collector:${version}"
    docker pull "$collector_image"
    tmp_output="$(mktemp)"

    echo "collector_image=${collector_image}"
    docker run --rm --entrypoint /bin/bash "$collector_image" -c '
        cd /kernel-modules &&
        head -n 1 MODULE_VERSION.txt &&
        ls *.gz
    ' >"$tmp_output"
    docker image rm --force "$collector_image"

    head -n 1 "$tmp_output" >"${version_dir}/MODULE_VERSION"
    tail -n +2 "$tmp_output" >"${version_dir}/INVENTORY"
    rm -f "$tmp_output" || true
done
