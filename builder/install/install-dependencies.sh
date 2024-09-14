#!/usr/bin/env bash

set -e

export LICENSE_DIR="/THIRD_PARTY_NOTICES"

mkdir -p "${LICENSE_DIR}"

NPROCS="$(nproc)"
export NPROCS

if [ "${USE_CCACHE}" = "true" ]; then
    # build and install ccache
    ./builder/install/ccache.sh

    # create a wrapper utility for clang-17 used by modern bpf builds in falco
    printf '#!/bin/sh\nexec ccache /usr/bin/clang-17 "$@"\n' > /usr/local/bin/ccache-clang
    chmod +x /usr/local/bin/ccache-clang
    echo /usr/local/bin/ccache-clang

    # Use ccache in cmake builds
    export CMAKE_C_COMPILER_LAUNCHER=ccache
    export CMAKE_CXX_COMPILER_LAUNCHER=ccache

    # print stats and zero them
    ccache -z -d /root/.ccache

    #export CCACHE_NOINODECACHE=true
    #export CCACHE_SLOPPINESS=time_macros,file_macro,include_file_mtime,include_file_ctime,gcno_cwd
    #export CCACHE_DEBUG=1
    #export CCACHE_DEBUGDIR=/ccache-debug
    #export CCACHE_LOGFILE=/ccache.log
    #mkdir "${CCACHE_DEBUGDIR}" || true
fi

# shellcheck source=SCRIPTDIR/versions.sh
source builder/install/versions.sh
for f in builder/install/[0-9][0-9]-*.sh; do
    echo "=== $f ==="
    ./"$f"
    ldconfig
done

if [ "${USE_CCACHE}" = "true" ]; then
    echo "=== ccache stats after build ==="
    ccache -s -d /root/.ccache
fi
