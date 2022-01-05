#!/usr/bin/env bash
set -eou pipefail

name=$1
flavor=$2
lifespan=$3

echo "Creating an infra cluster with name $name, flavor $flavor"

infractl create "$flavor" "$name" --description "Performance testing cluster"
infractl lifespan "$name" "$lifespan"
