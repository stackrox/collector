#!/usr/bin/env bash

set -euo pipefail

# This script creates a mock cache of drivers, meaning the actual drivers
# aren't downloaded, but the rest of the scripts will think they are.

for module_version_dir in /tmp/kobuild-tmp/versions-src/*; do
    module_version="$(basename "${module_version_dir}")"
    mkdir -p "/tmp/kernel-modules/$module_version/"

    gsutil ls "gs://collector-modules-public/${module_version}/" \
        | xargs basename \
        | awk -v version="${HOME}" '{print "/tmp/kernel-modules/" version "/" $1}' \
        | xargs touch
done
