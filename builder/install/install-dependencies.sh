#!/usr/bin/env bash

set -e

export LICENSE_DIR="/THIRD_PARTY_NOTICES"

mkdir -p "${LICENSE_DIR}"

export NPROCS
NPROCS="$(nproc)"

# shellcheck source=SCRIPTDIR/versions.sh
source builder/install/versions.sh
for f in builder/install/[0-9][0-9]-*.sh; do
    echo "=== $f ==="
    ./"$f"
    ldconfig
done
