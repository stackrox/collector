#!/usr/bin/env bash

set -e

if [ ! -n "${CPAAS_BUILD}" ]; then
    exit 0
fi

cp elfutils-${ELFUTILS_VERSION}-generated/libcpu/* elfutils/libcpu/
cp elfutils-${ELFUTILS_VERSION}-generated/libdw/* elfutils/libdw/
cd elfutils
cp AUTHORS "${LICENSE_DIR}/elfutils-${ELFUTILS_VERSION}"
autoreconf -i -f
./configure --prefix="/usr/local" --disable-maintainer-mode --disable-debuginfod
make -j "${NPROCS:-2}"
make install
