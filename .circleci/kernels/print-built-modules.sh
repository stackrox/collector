#!/usr/bin/env bash
set -eo pipefail

NODE_INDEX=$1

shard_output_dir="${WORKSPACE_ROOT}/ko-build/build-output/shard-${NODE_INDEX}"
for i in "${WORKSPACE_ROOT}/ko-build/module-versions"/*/; do
  version="$(basename "$i")"
  if [[ -d "${shard_output_dir}/$version" ]]; then
    find "${shard_output_dir}/${version}" -type f -ls
  fi
done
