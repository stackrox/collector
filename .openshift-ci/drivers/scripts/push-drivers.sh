#! /usr/bin/env bash

set -eo pipefail

GCP_CREDS="$(cat /tmp/secrets/GOOGLE_CREDENTIALS_KERNEL_CACHE)"
GCP_CHECK_BUCKET="gs://collector-build-cache"

/scripts/setup-gcp-env "$GCP_CREDS" "$GCP_CHECK_BUCKET"

target="gs://mauro-drivers-test/drivers"
BRANCH="mauro-OSCI-driver-builds"
BUILD_NUM=12345
if [[ "$BRANCH" != "master" && -z "$TAG" ]]; then
    target="gs://stackrox-collector-modules-staging/pr-builds/${BRANCH}/${BUILD_NUM}"
fi

shopt -s nullglob
shopt -s dotglob
for probes_dir in "/kernel-modules/"/*; do
    files=("${probes_dir}"/*.{gz,unavail})
    [[ "${#files[@]}" -gt 0 ]] || continue
    printf '%s\n' "${files[@]}" | gsutil -m cp -n -I "${target}/$(basename "$probes_dir")/"
done
