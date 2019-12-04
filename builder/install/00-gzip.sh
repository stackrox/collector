#!/usr/bin/env bash

set -e
exit 0

if [ ! -f /etc/redhat-release ]; then
 echo >&2 "Not building gzip from source"
 exit 0
fi

wget "https://ftp.gnu.org/gnu/gzip/gzip-${GZIP_VERSION}.tar.gz"
tar -xzf "gzip-${GZIP_VERSION}.tar.gz"
cd "gzip-${GZIP_VERSION}"
./configure --prefix="/usr/local"
make -j "${NPROCS:-2}"
make install
