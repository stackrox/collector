#!/usr/bin/env bash

set -euxo pipefail

# This script creates a mock cache of drivers, meaning the actual drivers
# aren't downloaded, but the rest of the scripts will think they are.

for module_version_dir in /tmp/kobuild-tmp/versions-src/*; do
    module_version="$(basename "${module_version_dir}")"
    mkdir -p "/tmp/kernel-modules/$module_version/"

    gsutil ls "gs://collector-modules-public/${module_version}/" \
        | awk -F "/" -v version="${module_version}" '{print "/tmp/kernel-modules/" version "/" $NF}' \
        | xargs touch
done
