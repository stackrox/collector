#! /usr/bin/env bash

set -euo pipefail

METADATA_DIR="/tmp/cpaas-support-packages/metadata/module-versions/"
OUTPUT_DIR=" /tmp/cpaas-support-packages/output/"

# Create the metadata directories
for version in "/kernel-modules"/*/; do
    md_version="$(basename "${version}")"
    mkdir -p "${METADATA_DIR}/$md_version"
done

/scripts/create-support-packages.sh \
      /LICENSE \
      gs://stackrox-collector-modules-staging/cpaas/ \
      "${METADATA_DIR}" \
      "${OUTPUT_DIR}"

# Sync support packages
gsutil -m rsync -r "${OUTPUT_DIR}" gs://sr-roxc/collector/support-packages/cpaas

# Sleep for 120s
for _ in {1..120}; do
      sleep 1
      echo -n .
done

# Delete old files
gsutil -m rsync -n -r -d "${OUTPUT_DIR}" gs://sr-roxc/collector/support-packages/cpaas
