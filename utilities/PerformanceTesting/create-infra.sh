#!/usr/bin/env bash
set -eou pipefail

name=$1
nodes=$2
flavor=$3
lifespan=$4

does_cluster_exist() {
    nline="$({ infractl get "$name" 2> /dev/null || true; } | wc -l)"
    if (("$nline" == 0)); then
        return 1
    else
        return 0
    fi
}

if does_cluster_exist; then
    echo "Unable to create cluster"
else
    echo "Creating an infra cluster with name '$name' and flavor '$flavor'"
    infractl create "$flavor" "$name" --description "Performance testing cluster --arg nodes=$nodes"
    infractl lifespan "$name" "$lifespan"
fi
