#!/usr/bin/env bash

set -euo pipefail

function retry {
    local n=1
    local max=5
    local delay=5
    while true; do
        # shellcheck disable=SC2015 # an info note, break is needed here to stop
        # after the command was successfully executed
        "$@" && break || {
            if [[ $n -lt $max ]]; then
                ((n++))
                echo "Command failed. Attempt $n/$max:"
                sleep $delay
            else
                echo "The command has failed after $n attempts." >&2
                exit 1
            fi
        }
    done
}
