#!/usr/bin/env bash

set -euo pipefail

die() {
    echo >&2 "$@"
    exit 1
}

function retry {
    local n=1
    local max=5
    local delay=5
    while true; do
        # shellcheck disable=SC2015 # an info note, break is needed here to stop
        # after the command was successfully executed
        "$@" && break || {
            if [[ $n -lt $max ]]; then
                ((n++))
                echo "Command failed. Attempt $n/$max:"
                sleep $delay
            else
                echo "The command has failed after $n attempts." >&2
                exit 1
            fi
        }
    done
}

MD_DIR="$1"

[[ -n "$MD_DIR" ]] || die "Usage: $0 <metadata directory>"
[[ -d "$MD_DIR" ]] || die "Metadata directory $MD_DIR does not exist or is not a directory."

for version_dir in "${MD_DIR}/collector-versions"/*; do
    [[ -d "$version_dir" ]] || continue
    version="$(basename "$version_dir")"

    collector_image="collector.stackrox.io/collector:${version}"
    retry docker pull "$collector_image"
    tmp_output="$(mktemp)"

    docker run --rm --entrypoint /bin/bash "$collector_image" -c '
        cd /kernel-modules &&
        head -n 1 MODULE_VERSION.txt &&
        ls *.gz
    ' > "$tmp_output"

    head -n 1 "$tmp_output" > "${version_dir}/MODULE_VERSION"
    tail -n +2 "$tmp_output" > "${version_dir}/INVENTORY"
    rm -f "$tmp_output" || true
done
