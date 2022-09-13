#! /usr/bin/env bash

set -euo pipefail

# shellcheck source=SCRIPTDIR/lib.sh
source /scripts/lib.sh

METADATA_DIR="/tmp/cpaas-support-packages/metadata"
OUTPUT_DIR="/tmp/cpaas-support-packages/output"
GCP_BASE_BUCKET=$1

# Create the metadata directories
for version in "/kernel-modules"/*/; do
    md_version="$(basename "${version}")"
    mkdir -p "${METADATA_DIR}/module-versions/$md_version"
done

target="${GCP_BASE_BUCKET}"

if is_in_PR_context; then
    BRANCH="$(get_branch)"
    target="gs://stackrox-collector-modules-staging/pr-builds/${BRANCH}/${BUILD_ID}"
fi

/scripts/create-support-packages.sh \
      /LICENSE \
      "${target}" \
      "${METADATA_DIR}" \
      "${OUTPUT_DIR}"

# Sync support packages
gsutil -m rsync -r "${OUTPUT_DIR}" "${target}/cpaas"

# Sleep for 120s
for _ in {1..120}; do
      sleep 1
      echo -n .
done

# Delete old files
gsutil -m rsync -n -r -d "${OUTPUT_DIR}" "${target}/cpaas"
