#!/usr/bin/env bash

set -e

cd third_party/libb64

cat AUTHORS LICENSE > "${LICENSE_DIR}/libb64-${B64_VERSION}"

CFLAGS=-fPIC make all_base64

cp src/libb64.a /usr/local/lib/
cp -r include/b64 /usr/local/include/
