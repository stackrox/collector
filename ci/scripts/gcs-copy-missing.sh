#!/usr/bin/env bash

DIR="$(cd "$(dirname "$0")" && pwd)"

source "${DIR}/common.sh"

list_files() {
    if [[ "$1" =~ ^gs:// ]]; then
        "${DIR}/gcs-list.sh" "$1"
    else
        "${DIR}/fs-list.sh" "$1"
}

[[ $# == 2 ]] || die "Usage: $0 <source> <destination>"

source="$(normalize_slashes "$1")"
dest="$(normalize_slashes "$2")"

[[ "$source" =~ ^gs:// || "$dest" =~ ^gs:// ]] || die "At least one of source and destination must be a GCS URL"

gsutil -m cp -n -I "${dest}/" < <(
    comm -23 <(list_files "$source") <(list_files "$dest") | "${DIR}/strfmt.sh" "${source}/")
