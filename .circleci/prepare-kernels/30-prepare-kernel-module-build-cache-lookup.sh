#!/usr/bin/env bash
set -eo pipefail

mkdir -p /tmp/cache
if [[ -f pr-metadata/labels/no-cache ]]; then
    echo > /tmp/cache/kernel-modules-version.txt
else
    echo "$MODULE_VERSION" > /tmp/cache/kernel-modules-version.txt
fi
