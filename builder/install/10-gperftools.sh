#!/usr/bin/env bash

set -e

cd third_party/gperftools
cp COPYING "${LICENSE_DIR}/gperftools-${GPERFTOOLS_VERSION}"

./autogen.sh
CXXFLAGS=${EXTRA_CFLAGS_DEBUG} CFLAGS=${EXTRA_CFLAGS_DEBUG} ./configure --enable-shared=no --prefix=/usr/local
make ${NPROCS:+-j ${NPROCS}}
make install
