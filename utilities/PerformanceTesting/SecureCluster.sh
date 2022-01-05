#!/usr/bin/env bash
set -eou pipefail

artifacts_dir=$1
collector_image_registry=${1:-docker.io/stackrox}
collector_image_tag=${2:-3.5.x-76-gaca574d047}

echo "Starting secure cluster services"

export KUBECONFIG=$artifacts_dir/kubeconfig

helm install -n stackrox stackrox-secured-cluster-services rhacs/secured-cluster-services \
  -f perf-bundle.yml \
  --set imagePullSecrets.username="$DOCKER_USERNAME" \
  --set imagePullSecrets.password="$DOCKER_PASSWORD" \
  --set clusterName=perf-test \
  --set image.collector.registry="$collector_image_registry" \
  --set image.collector.name=collector \
  --set image.collector.tag="$collector_image_tag" \
  --set enableOpenShiftMonitoring=true \
  --set exposeMonitoring=true
