#!/usr/bin/env bash

set -e

if [ -n "${CPAAS_BUILD}" ]; then
    tar xzf libb64-${LIBB64_VERSION}.tar.gz
    cd libb64-${LIBB64_VERSION}
else
    wget "https://downloads.sourceforge.net/project/libb64/libb64/libb64/libb64-${B64_VERSION}.zip"
    unzip "libb64-${B64_VERSION}.zip"
    cd "libb64-${B64_VERSION}"
fi
cat AUTHORS LICENSE > "${LICENSE_DIR}/libb64-${B64_VERSION}"
CFLAGS=-fPIC make all_base64
cp src/libb64.a /usr/local/lib/
cp -r include/b64 /usr/local/include/
