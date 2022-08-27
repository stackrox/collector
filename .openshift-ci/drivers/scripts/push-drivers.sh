#! /usr/bin/env bash

set -eo pipefail

# shellcheck source=SCRIPTDIR/lib.sh
source /scripts/lib.sh

BRANCH_DRIVER_CACHE="gs://stackrox-collector-modules-staging/pr-builds"

upload_drivers() {
    local drivers_dir=$1
    local target=$2

    for driver_version_dir in "${drivers_dir}"/*; do
        files=("${driver_version_dir}"/*.{gz,unavail})
        [[ "${#files[@]}" -gt 0 ]] || continue
        printf '%s\n' "${files[@]}" | gsutil -m cp -n -I "${target}/$(basename "${driver_version_dir}")/"
    done
}

GCP_CREDS="$(cat /tmp/secrets/GOOGLE_CREDENTIALS_KERNEL_CACHE)"
GCP_BASE_BUCKET="gs://collector-modules-osci"

/scripts/setup-gcp-env.sh "${GCP_CREDS}" "${GCP_BASE_BUCKET}"

target="${GCP_BASE_BUCKET}"

if is_in_PR_context; then
    BRANCH="$(get_branch)"
    target_base="${BRANCH_DRIVER_CACHE}/${BRANCH}"
    if ((PER_BRANCH_CACHE)); then
        target="${target_base}/branch"
    else
        target="${target_base}/${BUILD_ID}"
    fi
fi
echo "uploading built-drivers to ${target}"

shopt -s nullglob
shopt -s dotglob
upload_drivers "/built-drivers/" "${target}"

if ! is_in_PR_context; then
    # On tags/master builds, additionally upload modules from cache
    upload_drivers "/kernel-modules/" "${target}"
fi
