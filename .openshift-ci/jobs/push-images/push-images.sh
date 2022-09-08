#!/usr/bin/env bash
set -eo pipefail

export PROJECT_DIR=/go/src/github.com/stackrox/collector
# shellcheck source=SCRIPTDIR/../../drivers/scripts/lib.sh
source "$PROJECT_DIR/.openshift-ci/drivers/scripts/lib.sh"

push_images_to_repos() {
    local -n local_image_repos=$1
    local -n local_tags=$2
    local image_name=$3
    local osci_image=$4

    for repo in "${local_image_repos[@]}"; do
        registry_rw_login "$repo"

        for tag in "${local_tags[@]}"; do
            image="${repo}/${image_name}:${tag}"
            echo "Pushing image ${image}"
            oc image mirror "${osci_image}" "${image}"
        done
    done
}

push_builder_image() {

    BRANCH="$(get_branch)"
    tags=("$collector_version")

    if [[ "$BRANCH" == "master" ]]; then
        # shellcheck disable=SC2034
        tags+=("cache")
    fi

    oc registry login

    push_images_to_repos image_repos tags collector-builder "${COLLECTOR_BUILDER}"
}

push_images() {
    oc registry login

    # shellcheck disable=SC2034
    full_tags=(
        "${collector_version}"
        "${collector_version}-latest"
    )
    push_images_to_repos image_repos full_tags collector "${COLLECTOR_FULL}"

    # shellcheck disable=SC2034
    base_tags=(
        "${collector_version}-slim"
        "${collector_version}-base"
    )
    push_images_to_repos image_repos base_tags collector "${COLLECTOR_SLIM}"
}

cd "$PROJECT_DIR"

collector_version="$(make tag)"

export PUBLIC_REPO=quay.io/stackrox-io
export QUAY_REPO=quay.io/rhacs-eng

shopt -s nullglob
for cred in /tmp/secret/**/[A-Z]*; do
    export "$(basename "$cred")"="$(cat "$cred")"
done

# shellcheck source=SCRIPTDIR/registry_rw_login.sh
source "${PROJECT_DIR}/.openshift-ci/jobs/push-images/registry_rw_login.sh"

# Note that shellcheck reports unused variable when arrays are passed as reference.
# See https://github.com/koalaman/shellcheck/issues/1957
# shellcheck disable=SC2034
image_repos=(
    "${QUAY_REPO}"
    "${PUBLIC_REPO}"
)

push_images
push_builder_image
