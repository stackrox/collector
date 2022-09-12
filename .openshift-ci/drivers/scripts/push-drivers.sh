#! /usr/bin/env bash

set -exo pipefail

# shellcheck source=SCRIPTDIR/../../scripts/lib.sh
source /scripts/lib.sh

upload_drivers() {
    local drivers_dir=$1
    local target=$2

    for driver_version_dir in "${drivers_dir}"/*; do
        files=("${driver_version_dir}"/*.{gz,unavail})
        [[ "${#files[@]}" -gt 0 ]] || continue
        printf '%s\n' "${files[@]}" | gsutil -m cp -n -I "${target}/$(basename "${driver_version_dir}")/"
    done
}

GCP_CREDS="$(get_secret_file GOOGLE_CREDENTIALS_KERNEL_CACHE)"
GCP_BASE_BUCKET=$1

/scripts/gcloud-init.sh "${GCP_CREDS}" "${GCP_BASE_BUCKET}"

target="${GCP_BASE_BUCKET}"

if is_in_PR_context; then
    BRANCH="$(get_branch)"
    target="gs://stackrox-collector-modules-staging/pr-builds/${BRANCH}/${BUILD_ID}"
fi

shopt -s nullglob
shopt -s dotglob
upload_drivers "/built-drivers/" "${target}"

# Commented out the condition for tests, shouldn't be needed once merged since
# periodics don't run in PR contexts and will always push '/kernel-modules/'
# if ! is_in_PR_context; then
    # On tags/master builds, additionally upload modules from cache
    upload_drivers "/kernel-modules/" "${target}"
# fi
