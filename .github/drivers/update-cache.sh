#!/usr/bin/env bash

set -euo pipefail

for module_version_dir in /tmp/kobuild-tmp/versions-src/*; do
    module_version="$(basename "${module_version_dir}")"
    mkdir -p "/tmp/kernel-modules/$module_version/"

    gsutil -m rsync -r \
        "gs://collector-modules-osci/$module_version/" \
        "/tmp/kernel-modules/$module_version/" || true
done

BASE_DIR=/tmp \
    "${GITHUB_WORSPACE}/.openshift-ci/drivers/scripts/sanitize-drivers.py"
