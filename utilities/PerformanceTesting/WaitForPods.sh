#!/usr/bin/env bash
set -eoux pipefail

echo "Waiting for all pods to be running"

artifacts_dir=$1

export KUBECONFIG=$artifacts_dir/kubeconfig

while true
do
  #npods="$(kubectl -n stackrox get pods | wc -l)"
  #npods=$(( npods-1 ))
  #nrunning="$({ kubectl -n stackrox get pods | awk '{print $2}' | awk -F "/" '{if ($1 == $2) print}' || true ; } | wc -l)"
  pod_ready="$(kubectl -n stackrox get pod -o jsonpath='{.items[*].status.containerStatuses[*].ready}')"
  has_ready="$({ echo "$pod_ready" | grep true || true ; } | wc -l)"
  has_unready="$({ echo "$pod_ready" | grep false || true ; } | wc -l)"
  if [[ (( $has_ready == 1 )) && (( $has_unready == 0 )) ]]; then
    echo "All pods are running"
    break
  fi
  sleep 1
done
