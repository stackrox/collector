#!/usr/bin/env bash
set -eo pipefail

docker run --pull always --rm -i hadolint/hadolint < \
    "${SOURCE_ROOT}/collector/container/Dockerfile.ubi"
