#!/usr/bin/env bash
set -eo pipefail

dockerized=$1
image_family=$2
trigger_source=$3
branch=$4
tag=$5

log() { echo "$*" >&2; }

if [[ -f "${WORKSPACE_ROOT}/pr-metadata/labels/skip-integration-tests" ]]; then
    log "Skipping job with skip-integration-tests label." >&2
    exit 0
fi

if [[ "$dockerized" == "true" && ! -f "${WORKSPACE_ROOT}/pr-metadata/labels/run-dockerized-steps" ]]; then
    log "Skipping dockerized build jobs." >&2
    exit 0
fi

if [[ "$branch" != "master" && -z "$tag" && "$trigger_source" != "schedule" ]]; then
    if [[ "$image_family" =~ (ubuntu|rhel) ]]; then
        exit 1
    fi
    log "Skipping job for pr. Running only for master branch and nightly job." >&2
    log "If you want to run all integration tests use the all-integration-tests label" >&2
    exit 0
fi

exit 1
