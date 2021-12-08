#!/usr/bin/env bash
set -eou pipefail

name=$1

echo "Waiting for infra cluster to be ready" 

while true
do
  state=`infractl get $name | grep Status | awk '{print $2}'`
  if [[ "$state" == "READY" ]]; then
    break
  fi
done
