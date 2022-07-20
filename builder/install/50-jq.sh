#!/usr/bin/env bash

set -e

if [ -n "${WITH_RHEL8_RPMS}" ]; then
    # Already installed as an RPM, nothing more to do.
    exit 0
fi

wget "https://github.com/stedolan/jq/releases/download/jq-${JQ_VERSION}/jq-${JQ_VERSION}.tar.gz"
tar -zxf "jq-${JQ_VERSION}.tar.gz"
cd "jq-${JQ_VERSION}"
cp COPYING "${LICENSE_DIR}/jq-${JQ_VERSION}"
./configure --without-oniguruma --disable-maintainer-mode --enable-all-static --disable-dependency-tracking --prefix=/usr/local
make ${NPROCS:+-j ${NPROCS}} LDFLAGS=-all-static CFLAGS=-fPIC
make install
