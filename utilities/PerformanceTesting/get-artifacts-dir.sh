#!/usr/bin/env bash
set -eou pipefail

name=$1
artifacts_dir=$2

rm -rf "$artifacts_dir"
infractl artifacts "$name" --download-dir "$artifacts_dir"
