#!/usr/bin/env bash

use_downstream() {
    IFS='.' read -ra version <<< "${1%-*}"
    min_version=(2 6 0)

    for ((i = 0; i < ${#min_version[@]}; i++)); do
        if ((version[i] < min_version[i])); then
            return 1
        fi
    done

    return 0
}
