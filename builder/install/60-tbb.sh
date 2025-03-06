#!/usr/bin/env bash

set -e

if [ -n "${WITH_RHEL_RPMS}" ]; then
    # Already installed as an RPM, nothing more to do.
    exit 0
fi

cd third_party/tbb
cp LICENSE.txt "${LICENSE_DIR}/tbb-${TBB_VERSION}"
cmake -B cmake-build -S . \
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_SHARED_LIBS=OFF \
    -DTBB_TEST=OFF \
    -DTBBMALLOC_BUILD=OFF
cmake --build cmake-build --target install ${NPROCS:+-j ${NPROCS}}
