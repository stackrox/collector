#!/usr/bin/env bash
set -eox pipefail

SOURCE_ROOT=$1
SUPPORT_PKG_SRC_ROOT=$2

"${SUPPORT_PKG_SRC_ROOT}/01-collector-to-rox-version-map.py" \
        "${SOURCE_ROOT}/RELEASED_VERSIONS" \
        /tmp/support-packages/metadata

ls /tmp/support-packages/metadata

"${SUPPORT_PKG_SRC_ROOT}/02-fetch-collectors-metadata.sh" \
        /tmp/support-packages/metadata

"${SUPPORT_PKG_SRC_ROOT}/03-group-by-module-version.sh" \
        /tmp/support-packages/metadata

"${SUPPORT_PKG_SRC_ROOT}/04-create-support-packages.sh" \
        "${LICENSE_FILE}" /tmp/support-packages/metadata /tmp/support-packages/output

"${SUPPORT_PKG_SRC_ROOT}/05-create-index.py" \
        /tmp/support-packages/metadata /tmp/support-packages/output
