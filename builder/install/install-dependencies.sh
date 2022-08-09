#!/usr/bin/env bash

set -e

export LICENSE_DIR="/THIRD_PARTY_NOTICES"

mkdir -p "${LICENSE_DIR}"



# shellcheck source=SCRIPTDIR/versions.sh
source builder/install/versions.sh
for f in builder/install/[0-9][0-9]-*.sh; do
    echo "=== $f ==="
    #export CXX=/root/AFL/afl-g++
    #export CC=/root/AFL/afl-gcc
    export CXX=/root/AFLplusplus/afl-g++
    export CC=/root/AFLplusplus/afl-gcc
    ./"$f"
    ldconfig
done
