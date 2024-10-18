#!/usr/bin/env bash

set -e

if [ "${USE_CCACHE}" = "true" ]; then
    export CC="ccache gcc"
    export CXX="ccache g++"
fi

cd third_party/gperftools
cp COPYING "${LICENSE_DIR}/gperftools-${GPERFTOOLS_VERSION}"

./autogen.sh
./configure --enable-shared=no --prefix=/usr/local
make ${NPROCS:+-j ${NPROCS}}
make install
