#!/usr/bin/env bash
set -eo pipefail

rm -rf "${WORKSPACE_ROOT}/ko-build/build-output/FAILURES"-* 2>/dev/null || true
rm -rf "${WORKSPACE_ROOT}/ko-build/build-output/shard"-* 2>/dev/null || true
