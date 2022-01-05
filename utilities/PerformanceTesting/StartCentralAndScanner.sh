#!/usr/bin/env bash
set -eou pipefail

echo "Starting central and scanner related pods"

artifacts_dir=$1

export KUBECONFIG="$artifacts_dir"/kubeconfig
admin_password="$(cat "$artifacts_dir"/kubeadmin-password)"

helm install -n stackrox stackrox-central-services --create-namespace rhacs/central-services \
  --set central.exposure.route.enabled=true \
  --set central.adminPassword.value="$admin_password" \
  --set imagePullSecrets.username="$DOCKER_USERNAME" \
  --set imagePullSecrets.password="$DOCKER_PASSWORD" \
  --set enableOpenShiftMonitoring=true \
  --set central.exposeMonitoring=true
