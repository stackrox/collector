#!/usr/bin/env bash

set -e

wget "https://downloads.sourceforge.net/project/libb64/libb64/libb64/libb64-${B64_VERSION}.zip"
unzip "libb64-${B64_VERSION}.zip"
cd "libb64-${B64_VERSION}"
CFLAGS=-fPIC make
cp src/libb64.a /usr/local/lib/
cp -r include/b64 /usr/local/include/
