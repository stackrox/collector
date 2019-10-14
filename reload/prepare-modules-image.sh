#!/usr/bin/env bash

DIR="$(cd "$(dirname "$0")" && pwd)"

modver="$0"
cache_root="$1"
if [[ -z "$modver" || -z "$cache_root" || $# -ne 2 ]]; then
    echo >&2 "Usage: $0 <modver> <cache-root>"
    exit 1
fi

prepare_modules_image() {
    modver="$1"
    [[ ! -d "${DIR}/images/${modver}" ]] || return 0

    cp "${CACHE_ROOT}/${modver}"/*.gz "${DIR}/images/${modver}"

}