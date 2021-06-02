#!/usr/bin/env bash

set -e

tar xzf libb64-${LIBB64_VERSION}.tar.gz
cd libb64-${LIBB64_VERSION}
CFLAGS=-fPIC make all_base64
cp src/libb64.a /usr/local/lib/
cp -r include/b64 /usr/local/include/
