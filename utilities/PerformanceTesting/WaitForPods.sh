#!/usr/bin/env bash
set -eoux pipefail

echo "Waiting for all pods to be running"

artifacts_dir=$1

export KUBECONFIG=$artifacts_dir/kubeconfig

while true
do
  npods="$(kubectl -n stackrox get pods | wc -l)"
  npods=$(( npods-1 ))
  nrunning="$({ kubectl -n stackrox get pods | awk '{print $3}' | grep Running || true ; } | wc -l)"
  echo "npods= $npods nrunning= $nrunning"
  if [[ $npods == "$nrunning" ]]; then
    echo "All pods are running"
    break
  fi
  sleep 1
done
