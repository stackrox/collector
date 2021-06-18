#!/usr/bin/env bash

set -e

if [ -n "${WITH_RHEL8_RPMS}" ]; then
    cd libb64
else
    wget "https://downloads.sourceforge.net/project/libb64/libb64/libb64/libb64-${B64_VERSION}.zip"
    unzip "libb64-${B64_VERSION}.zip"
    cd "libb64-${B64_VERSION}"
fi
cat AUTHORS LICENSE > "${LICENSE_DIR}/libb64-${B64_VERSION}"
CFLAGS=-fPIC make all_base64
cp src/libb64.a /usr/local/lib/
cp -r include/b64 /usr/local/include/
