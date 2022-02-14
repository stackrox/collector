#!/usr/bin/env bash
set -eo pipefail

mkdir -p "/tmp/cache/kernel-modules/${MODULE_VERSION}/"
cp -rl "${SOURCE_ROOT}/kernel-modules/container/kernel-modules/." \
    "/tmp/cache/kernel-modules/${MODULE_VERSION}/"
echo "$MODULE_VERSION" > /tmp/cache/kernel-modules-version.txt
