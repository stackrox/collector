#!/usr/bin/env bash

set -euo pipefail

base_dir="$(dirname "$0")"

mkdir -p source-archives/
gsutil -m rsync -r 'gs://stackrox-kernel-modules-source/collector' source-archives/

mkdir -p sources/
for f in source-archives/*.tgz; do
    version="$(basename "$f" .tgz)"
    if ! mkdir "sources/${version}"; then
        echo >&2 "Directory for module version ${version} already exists, remove to overwrite."
        continue
    fi

    echo "Extracting module version ${version} ..."
    tar -xzf "$f" -C "sources/${version}"

    patch_file="${base_dir}/../kernel-modules/patches/${version}.patch"
    if [[ -f "$patch_file" ]]; then
        echo "Patching module version ${version} ..."
        patch -p1 -d "sources/${version}" < "$patch_file"
    fi
done
