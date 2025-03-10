#!/usr/bin/env bash

set -e

export LICENSE_DIR="/THIRD_PARTY_NOTICES"

mkdir -p "${LICENSE_DIR}"

export NPROCS
NPROCS="${NPROCS:-$(nproc)}"

export CMAKE_BUILD_TYPE
CMAKE_BUILD_TYPE=Release
export EXTRA_CFLAGS_DEBUG
if [ -n "${COLLECTOR_BUILDER_DEBUG}" ]; then
    CMAKE_BUILD_TYPE=RelWithDebInfo
    EXTRA_CFLAGS_DEBUG=-g
fi

# shellcheck source=SCRIPTDIR/versions.sh
source builder/install/versions.sh
for f in builder/install/[0-9][0-9]-*.sh; do
    echo "=== $f ==="
    ./"$f"
    ldconfig
done
