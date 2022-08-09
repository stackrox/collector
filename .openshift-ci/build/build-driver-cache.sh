#! /usr/bin/env bash

set -euo pipefail

# The driver images built by OSCI leave drivers in 2 places:
#   - /kernel-modules: This is the cache downloaded from GCP
#   - /built-drivers: This are all newly built drivers
#
# We need to use the /MODULE_VERSION file to copy only drivers for the given
# version into the /driver-cache directory, which will later by copied into a
# slim image of collector to create the latest or FULL image.

MODULE_VERSION="$(cat /MODULE_VERSION)"

mkdir /driver-cache
mkdir -p "/kernel-modules/${MODULE_VERSION}" "/built-drivers/${MODULE_VERSION}"
find "/kernel-modules/${MODULE_VERSION}" -type f -not -path "*/.*" -exec cp {} /driver-cache \;
find "/built-drivers/${MODULE_VERSION}" -type f -not -path "*/.*" -exec cp {} /driver-cache \;
