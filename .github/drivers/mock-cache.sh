#!/usr/bin/env bash

set -euo pipefail

# This script creates a mock cache of drivers, meaning the actual drivers
# aren't downloaded, but the rest of the scripts will think they are.

for module_version_dir in /tmp/kobuild-tmp/versions-src/*; do
    module_version="$(basename "${module_version_dir}")"
    mkdir -p "/tmp/kernel-modules/$module_version/"

    # If the bucket doesn't exist, we are building a new version of drivers
    if ! gsutil ls "gs://collector-modules-public/${module_version}/" 1>&2 2> /dev/null; then
        continue
    fi

    # The awk command is really ugly, but basically extracts the driver name
    # from the ls command and creates the path where the driver will be looked
    # for on next steps.
    gsutil ls "gs://collector-modules-public/${module_version}/" \
        | awk -F "/" -v version="${module_version}" '{print "/tmp/kernel-modules/" version "/" $NF}' \
        | xargs touch
done
