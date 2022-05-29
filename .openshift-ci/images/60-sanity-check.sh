#!/usr/bin/env bash
set -eo pipefail

cd "$SOURCE_ROOT"
echo '>>> Collector Artifacts:'
find collector/container
echo '>>> Kernel Modules:'
find kernel-modules/container
