#!/usr/bin/env bash
set -eoux pipefail

echo "Waiting for all pods to be running"

artifacts_dir=$1

export KUBECONFIG=$artifacts_dir/kubeconfig

while true
do
  pod_ready="$(kubectl -n stackrox get pod -o jsonpath='{.items[*].status.containerStatuses[*].ready}')"
  has_ready="$({ echo "$pod_ready" | grep true || true ; } | wc -l)"
  has_unready="$({ echo "$pod_ready" | grep false || true ; } | wc -l)"
  ready_containers="$(kubectl -n stackrox get pod -o jsonpath='{.items[*].status.containerStatuses[?(@.ready == true)]}')"
  not_ready_containers="$(kubectl -n stackrox get pod -o jsonpath='{.items[*].status.containerStatuses[?(@.ready == false)]}')"
  #if [[ (( $has_ready == 1 )) && (( $has_unready == 0 )) ]]; then
  if [[ -n "$ready_containers" && -z "$not_ready_containers" ]]; then
    echo "All pods are running"
    break
  fi
  sleep 1
done
