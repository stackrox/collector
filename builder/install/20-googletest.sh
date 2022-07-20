#!/usr/bin/env bash

set -e

cd third_party/googletest
cp LICENSE "${LICENSE_DIR}/googletest-${GOOGLETEST_REVISION}"
mkdir cmake-build
cd cmake-build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local ..
cmake --build . --target install ${NPROCS:+-j ${NPROCS}}
