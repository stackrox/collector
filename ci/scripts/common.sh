#!/usr/bin/env bash

normalize_slashes() {
    sed -E 's@([^:/]|^)/+(/|$)@\1\2@g' <<<"$1"
}

eecho() {
    echo >&2 "$@"
}

die() {
    eecho "$@"
    exit 1
}
