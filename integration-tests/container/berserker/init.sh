#!/bin/bash

set -eo pipefail

set -x

IP_BASE="${IP_BASE:-223.42.0.1/16}"

/scripts/prepare-tap.sh -a "$IP_BASE" -o

if [[ "$IS_CLIENT" == "true" ]]; then
    berserker /etc/berserker/network/client.toml &
else
    berserker /etc/berserker/network/server.toml &
fi

PID=$!

cleanup() {
    echo "Killing $PID"

    kill -9 "$PID"

    ip link delete berserker0

    exit
}

trap cleanup SIGINT SIGABRT

wait -n "$PID"

ip link delete berserker0
