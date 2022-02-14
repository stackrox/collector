#!/usr/bin/env bash
set -eo pipefail

docker run --rm -i hadolint/hadolint < \
    "${SOURCE_ROOT}/collector/container/Dockerfile.ubi"
