#!/usr/bin/env bash

# gcs-list.sh
# List files under a given bucket/path in a GCS bucket, with the bucket/path
# prefix removed.
#

DIR="$(cd "$(dirname "$0")" && pwd)"

source "${DIR}/common.sh"

if [[ $# < 1 || $# > 2 ]]; then
    eecho "Usage: $0 <gcs path> [<pattern>]"
    eecho "If not pattern is specified, it defaults to '**'"
    exit 1
fi

gcs_path="$(normalize_slashes "$1")/"
pattern="${2:-**}"

[[ ! "$pattern" =~ \.\. ]] || die "Pattern must not contain '..'"

gsutil ls "${gcs_path}${pattern}" | while IFS= read -r line || [[ -n "$line" ]]; do
    echo "${line#${gcs_path}}"
done
