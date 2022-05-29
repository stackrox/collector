#!/usr/bin/env bash
set -eo pipefail

gsutil -m cp -n \
    "${COLLECTOR_MODULES_BUCKET}/${MODULE_VERSION}/*.gz" \
    "${SOURCE_ROOT}/kernel-modules/container/kernel-modules" || true
