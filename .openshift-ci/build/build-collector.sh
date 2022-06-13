#!/usr/bin/env bash
set -eo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"

cp -r "${ROOT}" /tmp/collector


export DISABLE_PROFILING="true"
export SRC_ROOT_DIR=/tmp/collector

make -C /tmp/collector/collector pre-build
/build-collector.sh

make -C /tmp/collector/collector post-build
