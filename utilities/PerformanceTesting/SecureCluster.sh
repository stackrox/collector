#!/usr/bin/env bash
set -eou pipefail

artifacts_dir=$1
collector_image_registry=${2:-docker.io/stackrox}
collector_image_tag=${3:-3.5.x-76-gaca574d047}

echo "Starting secure cluster services"

export KUBECONFIG=$artifacts_dir/kubeconfig

helm install -n stackrox stackrox-secured-cluster-services rhacs/secured-cluster-services \
  -f perf-bundle.yml \
  --set collector.roxAfterglowPeriod=10 \
  --set imagePullSecrets.username="$DOCKER_USERNAME" \
  --set imagePullSecrets.password="$DOCKER_PASSWORD" \
  --set clusterName=perf-test \
  --set image.collector.registry="$collector_image_registry" \
  --set image.collector.name=collector \
  --set image.collector.tag="$collector_image_tag" \
  --set enableOpenShiftMonitoring=true \
  --set exposeMonitoring=true \
  --set image.main.registry=docker.io/stackrox \
  --set image.main.name=main \
  --set image.main.tag=3.68.x-198-g16f3ea3c01-dirty
  #--set image.main.tag=3.68.x-196-gc531c2675f
