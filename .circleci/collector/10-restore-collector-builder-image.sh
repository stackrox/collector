#!/usr/bin/env bash
set -eo pipefail

docker pull "quay.io/rhacs-eng/collector-builder:${COLLECTOR_BUILDER_TAG}"
