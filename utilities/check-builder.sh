#!/usr/bin/env bash

set -euo pipefail

BUILDER_NAME="$1"

CONTAINER_ENGINE="${CONTAINER_ENGINE:-docker}"
exists="$(${CONTAINER_ENGINE} ps -aqf name=^"${BUILDER_NAME}"$)"

if [[ -z "$exists" ]]; then
    echo >&2 "Collector builder is not created."
    echo >&2 "Run 'make start-builder'."
    exit 1
fi

running="$(${CONTAINER_ENGINE} container inspect -f '{{ .State.Running }}' "${BUILDER_NAME}")"
if [[ "${running}" != "true" ]]; then
    echo >&2 "Collector builder is not running."
    echo >&2 "Run '${CONTAINER_ENGINE} start ${BUILDER_NAME}'"
    exit 1
fi
