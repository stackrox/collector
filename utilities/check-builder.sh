#!/usr/bin/env bash

set -euo pipefail

BUILDER_NAME="$1"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/.."
CONTAINER_ENGINE="${CONTAINER_ENGINE:-docker}"

function builder_is_running() {
    ${CONTAINER_ENGINE} container inspect -f '{{ .State.Running }}' "${BUILDER_NAME}"
}

# Wait for up to 10 seconds until the container is running
function container_wait() {
    timeout=10
    until [[ "$(builder_is_running)" == "true" ]]; do
        timeout=$((timeout - 1))

        if ((timeout == 0)); then
            echo >&2 "Collector builder failed to start"
            exit 1
        fi

        sleep 1
    done
}

exists="$(${CONTAINER_ENGINE} ps -aqf name=^"${BUILDER_NAME}"$)"
if [[ -z "$exists" ]]; then
    echo "Collector builder is not created."
    echo "Attempting to start builder..."
    make -C "${ROOT_DIR}" start-builder
    container_wait
    exit 0
fi

if [[ "$(builder_is_running)" != "true" ]]; then
    echo "Collector builder is not running."
    echo "Attempting to start builder..."
    "${CONTAINER_ENGINE}" start "${BUILDER_NAME}"
    container_wait
fi
