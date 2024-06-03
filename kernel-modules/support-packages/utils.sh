#!/usr/bin/env bash

_check_min_version() {
    local version_re="^[0-9]+\.[0-9]+\.[0-9]+(:?-rc?[0-9]+)?$"

    if [[ ! "$1" =~ $version_re ]]; then
        echo >&2 "Error: Invalid version number format '$1'"
        exit 1
    fi

    if [[ ! "$2" =~ $version_re ]]; then
        echo >&2 "Error: Invalid version number format '$2'"
        exit 1
    fi

    IFS='.' read -ra version <<< "${1%-*}"
    IFS='.' read -ra min_version <<< "${2%-*}"

    for ((i = 0; i < ${#min_version[@]}; i++)); do
        if ((version[i] < min_version[i])); then
            return 1
        fi
    done

    return 0
}

use_downstream() {
    _check_min_version "$1" "2.6.0"
}

use_downstream_only() {
    _check_min_version "$1" "2.9.0"
}

dont_build_support_package() {
    _check_min_version "$1" "2.10.0"
}

bucket_has_drivers() {
    count=$(gsutil ls "$1" | wc -l)

    ((count != 0))
}
