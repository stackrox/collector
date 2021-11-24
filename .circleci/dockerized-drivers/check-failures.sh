#!/usr/bin/env bash
set -euo pipefail

DRIVER_REPO="$1"

mkdir -p /tmp/dockerized-failures

docker run --rm \
	-v /tmp/dockerized-failures:/tmp/dockerized-failures \
	-v "${SOURCE_ROOT}/kernel-modules/dockerized/scripts":/scripts:ro \
	"${DRIVER_REPO}/collector-drivers:${COLLECTOR_DRIVERS_TAG}" /scripts/unify-failures.sh
