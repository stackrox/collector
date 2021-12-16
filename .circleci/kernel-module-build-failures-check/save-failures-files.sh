#!/usr/bin/env bash
set -eo pipefail

mkdir /tmp/failures
cp -r "${WORKSPACE_ROOT}/ko-build/build-output/FAILURES"-*/. /tmp/failures || true
