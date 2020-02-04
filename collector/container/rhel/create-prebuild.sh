#!/usr/bin/env bash

set -euo pipefail

die() {
    echo >&2 "$@"
    exit 1
}

OUTPUT="$1"

[[ -n "$OUTPUT" ]] || die "Usage: $0 <output>"

touch "$OUTPUT"
chmod a+x "$OUTPUT"
