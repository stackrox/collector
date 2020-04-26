#!/usr/bin/env bash

DIR="$(cd "$(dirname "$0")" && pwd)"

source "${DIR}/common.sh"

if [[ $# < 1 || $# > 2 ]]; then
    eecho "Usage: $0 <path> [<pattern>]"
    eecho "If pattern is not specified, '*' is assumed"
    exit 1
fi

path="$1"
pattern="${2:-*}"

cd "${path}"
find . -name "$pattern" | cut -c 3-
