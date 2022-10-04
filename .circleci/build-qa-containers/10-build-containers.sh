#!/usr/bin/env bash
set -euo pipefail

# Build all containers used for QA.
# They can all be found under integration-tests/container

for subdir in "${SOURCE_ROOT}/integration-tests/container"/*/; do
    make -C "${subdir}"
done
