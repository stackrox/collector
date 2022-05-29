#!/usr/bin/env bash
set -eo pipefail

docker pull "stackrox/collector-builder:${COLLECTOR_BUILDER_TAG}"
