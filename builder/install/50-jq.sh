#!/usr/bin/env bash

set -e

wget "https://github.com/stedolan/jq/releases/download/jq-${JQ_VERSION}/jq-${JQ_VERSION}.tar.gz"
tar -zxf "jq-${JQ_VERSION}.tar.gz"
cd "jq-${JQ_VERSION}"
./configure --without-oniguruma --disable-maintainer-mode --enable-all-static --disable-dependency-tracking --prefix=/usr/local
make LDFLAGS=-all-static CFLAGS=-fPIC
make install
