#! /usr/bin/env bash

set -eo pipefail

# shellcheck source=SCRIPTDIR/lib.sh
source /scripts/lib.sh

GCP_CREDS="$(cat /tmp/secrets/GOOGLE_CREDENTIALS_KERNEL_CACHE)"
GCP_BASE_BUCKET="gs://collector-modules-osci"

/scripts/setup-gcp-env.sh "${GCP_CREDS}" "${GCP_BASE_BUCKET}"

target="${GCP_BASE_BUCKET}"

if is_in_PR_context; then
    BRANCH="$(get_base_ref)"
    target="${GCP_BASE_BUCKET}/pr-builds/${BRANCH}/${BUILD_ID}"
fi

shopt -s nullglob
shopt -s dotglob
for probes_dir in "/kernel-modules/"/*; do
    files=("${probes_dir}"/*.{gz,unavail})
    [[ "${#files[@]}" -gt 0 ]] || continue
    printf '%s\n' "${files[@]}" | gsutil -m cp -n -I "${target}/$(basename "$probes_dir")/"
done
