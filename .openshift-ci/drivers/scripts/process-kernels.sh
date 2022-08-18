#!/usr/bin/env bash
set -euo pipefail

# Download all cached driver for every version being built
build_cache() {
    for module_version_dir in /kobuild-tmp/versions-src/*; do
        module_version="$(basename "${module_version_dir}")"
        mkdir -p "/kernel-modules/$module_version/"

        gsutil -m rsync -r \
            "gs://collector-modules-osci-public/$module_version/" \
            "/kernel-modules/$module_version/" || true
    done

    /scripts/sanitize-drivers.py
}

mkdir -p "/kernel-modules"
((NO_CACHE)) || build_cache

# Create the tasks file
KERNELS_FILE=/kernels/all /scripts/get-build-tasks.sh

rm -f /all-build-tasks /non-blocklisted-build-tasks
mkdir -p /tasks/
mv /build-tasks /tasks/all
