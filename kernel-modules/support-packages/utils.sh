#!/usr/bin/env bash

use_downstream() {
    if [[ ! "$1" =~ ^[0-9]+\.[0-9]+\.[0-9]+(:?-rc?[0-9]+)?$ ]]; then
        echo >&2 "Error: Invalid version number format '$1'"
        exit 1
    fi

    IFS='.' read -ra version <<< "${1%-*}"
    min_version=(2 6 0)

    for ((i = 0; i < ${#min_version[@]}; i++)); do
        if ((version[i] < min_version[i])); then
            return 1
        fi
    done

    return 0
}
