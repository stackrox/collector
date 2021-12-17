#!/usr/bin/env bash
set -eou pipefail

artifacts_dir=$1

echo "Starting second cluster"

export KUBECONFIG=$artifacts_dir/kubeconfig

helm install -n stackrox stackrox-secured-cluster-services rhacs/secured-cluster-services \
  -f perf-bundle.yml \
  --set imagePullSecrets.username=$DOCKER_USERNAME \
  --set imagePullSecrets.password=$DOCKER_PASSWORD \
   --set clusterName=perf-test \
   --set enableOpenShiftMonitoring=true \
   --set exposeMonitoring=true
