#!/usr/bin/env bash
set -eo pipefail

SOURCE_ROOT=$1
SUPPORT_PKG_SRC_ROOT=$2
COLLECTOR_MODULES_BUCKET=${3:-"gs://collector-modules/612dd2ee06b660e728292de9393e18c81a88f347ec52a39207c5166b5302b656"}
LICENSE_FILE=${4:-"${SOURCE_ROOT}/collector/LICENSE-kernel-modules.txt"}

"${SUPPORT_PKG_SRC_ROOT}/01-collector-to-rox-version-map.py" \
        "${SOURCE_ROOT}/RELEASED_VERSIONS" \
        /tmp/support-packages/metadata

"${SUPPORT_PKG_SRC_ROOT}/02-fetch-collectors-metadata.sh" \
        /tmp/support-packages/metadata

"${SUPPORT_PKG_SRC_ROOT}/03-group-by-module-version.sh" \
        /tmp/support-packages/metadata

"${SUPPORT_PKG_SRC_ROOT}/04-create-support-packages.sh" \
        "${LICENSE_FILE}" "${COLLECTOR_MODULES_BUCKET}" /tmp/support-packages/metadata /tmp/support-packages/output

"${SUPPORT_PKG_SRC_ROOT}/05-create-index.py" \
        /tmp/support-packages/metadata /tmp/support-packages/output
