#!/usr/bin/env bash
set -eo pipefail

shopt -s nullglob
mod_build_dir="${WORKSPACE_ROOT}/ko-build/build-output"
for mod_dir in "${WORKSPACE_ROOT}/ko-build/module-versions"/*/; do
    mod_ver="$(basename "$mod_dir")"
    echo "Checking for module version ${mod_ver}"
    [[ -d "${mod_build_dir}/${mod_ver}" ]] || continue
    [[ -n "$( find "${mod_build_dir}/${mod_ver}" -type f -name '*.gz')"  ]] || continue
    echo "Copying into container build directory for module version ${mod_ver}"
    container_build_dir="${WORKSPACE_ROOT}/images/${mod_ver}/container"
    mkdir -p "${container_build_dir}/kernel-modules"
    cp "${mod_build_dir}/${mod_ver}"/*.gz "${container_build_dir}/kernel-modules"
done
