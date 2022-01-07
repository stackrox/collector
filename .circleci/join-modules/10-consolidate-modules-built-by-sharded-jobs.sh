#!/usr/bin/env bash
set -eo pipefail

shopt -s nullglob
shopt -s dotglob

mkdir -p "${WORKSPACE_ROOT}/ko-build/built-probes"
shard_dirs=("${WORKSPACE_ROOT}/ko-build/build-output/shard-"*)

if [[ "${#shard_dirs[@]}" == 0 ]]; then
  exit 0
fi

for shard_dir in "${shard_dirs[@]}"; do
  for version_dir in "${WORKSPACE_ROOT}/ko-build/module-versions"/*/; do
    version="$(basename "$version_dir")"
    job_dir="${shard_dir}/${version}"
    [[ -d "${job_dir}" ]] || continue
    out_dir="${WORKSPACE_ROOT}/ko-build/built-probes/${version}/"
    mkdir -p "$out_dir"
    files=("${job_dir}"/*.{gz,unavail})
    echo "${#files[@]} files in ${job_dir}"
    [[ "${#files[@]}" -gt 0 ]] || continue
    mv "${job_dir}"/*.{gz,unavail} "$out_dir"
  done
done
