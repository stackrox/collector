#!/usr/bin/env bash
set -euo pipefail

# shellcheck source=SCRIPTDIR/lib.sh
source /scripts/lib.sh

MAIN_DRIVER_CACHE="gs://collector-modules-osci-public/"
BRANCH_DRIVER_CACHE="gs://stackrox-collector-modules-staging/pr-builds"

# Download all cached driver for every version being built
build_cache_from_gcs() {
    cache_bucket=$1
    for module_version_dir in /kobuild-tmp/versions-src/*; do
        module_version="$(basename "${module_version_dir}")"
        mkdir -p "/kernel-modules/$module_version/"

        gsutil -m rsync -r \
            "$cache_bucket/$module_version/" \
            "/kernel-modules/$module_version/" || true
    done

    /scripts/sanitize-drivers.py
}

build_cache_from_branch() {
    branch_name="$(get_branch)"
    build_cache_from_gcs "$BRANCH_DRIVER_CACHE/$branch_name/branch"
}

build_cache_from_main() {
    build_cache_from_gcs "$MAIN_DRIVER_CACHE"
}

mkdir -p "/kernel-modules"
((NO_CACHE)) || build_cache_from_main
((PER_BRANCH_CACHE)) && build_cache_from_branch

# Create the tasks file
KERNELS_FILE=/kernels/all /scripts/get-build-tasks.sh

rm -f /all-build-tasks /non-blocklisted-build-tasks
mkdir -p /tasks/
mv /build-tasks /tasks/all
