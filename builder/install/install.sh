#!/usr/bin/env bash

set -e

export LICENSE_DIR="/THIRD_PARTY_NOTICES"

mkdir -p "${LICENSE_DIR}"
cd /install-tmp/
# shellcheck source=SCRIPTDIR/versions.sh
source ./versions.sh
for f in [0-9][0-9]-*.sh; do
    ./"$f"
done
cd && rm -rf /install-tmp
