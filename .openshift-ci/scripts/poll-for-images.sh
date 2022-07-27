#!/usr/bin/env bash

set -euo pipefail

function poll_for_collector_images() {
    info "Polling for images required for integration tests"

    local time_limit="$1"
    local tag
    tag="$(make --quiet tag)"
    local start_time
    start_time="$(date '+%s')"

    _image_exists() {
        local name="$1"
        local url="https://quay.io/api/v1/repository/rhacs-eng/$name/tag?specificTag=$tag"
        info "Checking for $name using $url"
        local check
        check=$(curl --location -sS -H "Authorization: Bearer ${QUAY_RHACS_ENG_BEARER_TOKEN}" "$url")
        echo "$check"
        [[ "$(jq -r '.tags | first | .name' <<< "$check")" == "$tag" ]]
    }

    while true; do
        if _image_exists "collector"; then
            info "Collector image exists"
            break
        fi
        if (($( date '+%s') - start_time > time_limit)); then
            die "Timed out waiting for images after ${time_limit} seconds"
        fi
        sleep 60
    done
}

# default to 2700 seconds (45 minutes)
poll_for_collector_images "${1:-2700}"
