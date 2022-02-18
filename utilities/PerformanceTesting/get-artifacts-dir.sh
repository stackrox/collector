#!/usr/bin/env bash
set -eou pipefail

name=$1
artifacts_dir=$2

rm -r "$artifacts_dir" || true
infractl artifacts "$name" --download-dir "$artifacts_dir"
