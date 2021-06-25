#!/usr/bin/env bash

set -e

source ./versions.sh

export LICENSE_DIR="/THIRD_PARTY_NOTICES"

mkdir -p "${LICENSE_DIR}"
cd /install-tmp/
for f in [0-9][0-9]-*.sh; do
  ./"$f"
done
cd && rm -rf /install-tmp
