#!/usr/bin/env bash

mkdir -p patches

for d in sources/*; do
    version="$(basename "$d")"
    archive="source-archives/${version}.tgz"
    if [[ ! -f "$archive" ]]; then
        echo >&2 "Missing source archive for module version $version, please run the download script"
        continue
    fi

    clean_dir="$(mktemp -d)"
    git -C "$clean_dir" init

    tar -xzf "$archive" -C "$clean_dir"
    git -C "$clean_dir" add .
    git -C "$clean_dir" commit --allow-empty -m "init"
    cp -r "$d"/* "$clean_dir"
    git -C "$clean_dir" add .
    git -C "$clean_dir" diff HEAD >"patches/${version}.patch"
done
