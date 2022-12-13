#!/usr/bin/env bash

set -euo pipefail

# TODO: Move the buckets back to the official ones once we are done testing

# This script creates a mock cache of drivers, meaning the actual drivers
# aren't downloaded, but the rest of the scripts will think they are.

for module_version_dir in /tmp/kobuild-tmp/versions-src/*; do
    module_version="$(basename "${module_version_dir}")"
    mkdir -p "/tmp/kernel-modules/$module_version/"

    # If the bucket doesn't exist, we are building a new version of drivers
    if ! gsutil ls "gs://mauro-drivers-test/drivers/${module_version}/" > /dev/null; then
        echo "Could not find cached drivers for '${module_version}'. Continuing with no cache"
        continue
    fi

    # The awk command is really ugly, but basically extracts the driver name
    # from the ls command and creates the path where the driver will be looked
    # for on next steps.
    gsutil ls "gs://mauro-drivers-test/drivers/${module_version}/" \
        | awk -F "/" -v version="${module_version}" '{print "/tmp/kernel-modules/" version "/" $NF}' \
        | xargs touch
done
