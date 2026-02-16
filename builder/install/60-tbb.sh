#!/usr/bin/env bash

set -eu

if [ -n "${WITH_RHEL_RPMS:-}" ]; then
    # Already installed as an RPM, nothing more to do.
    exit 0
fi

source builder/install/cmake.sh

cd third_party/tbb
cp LICENSE.txt "${LICENSE_DIR}/tbb-${TBB_VERSION}"

cmake_wrap "tbb" "${OUTPUT_DIR}" \
    -DBUILD_SHARED_LIBS=OFF \
    -DTBB_TEST=OFF \
    -DTBBMALLOC_BUILD=OFF
