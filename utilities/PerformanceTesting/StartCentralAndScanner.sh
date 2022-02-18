#!/usr/bin/env bash
set -eou pipefail

echo "Starting central and scanner related pods"

artifacts_dir=$1

export KUBECONFIG="$artifacts_dir"/kubeconfig
admin_password="$(cat "$artifacts_dir"/kubeadmin-password)"

settings=(
    --set central.exposure.route.enabled=true
    --set central.adminPassword.value="$admin_password"
    --set imagePullSecrets.username="$DOCKER_USERNAME"
    --set imagePullSecrets.password="$DOCKER_PASSWORD"
    --set enableOpenShiftMonitoring=true
)

if [[ -n ${CENTRAL_IMAGE_REGISTRY:-} ]]; then
    settings+=(--set central.image.registry="$CENTRAL_IMAGE_REGISTRY")
fi

if [[ -n ${CENTRAL_IMAGE_NAME:-} ]]; then
    settings+=(--set central.image.name="$CENTRAL_IMAGE_NAME")
fi

if [[ -n ${CENTRAL_IMAGE_TAG:-} ]]; then
    settings+=(--set central.image.tag="$CENTRAL_IMAGE_TAG")
fi

if [[ -n ${SCANNER_DBIMAGE_REGISTRY:-} ]]; then
    settings+=(--set scanner.dbImage.registry="$SCANNER_DBIMAGE_REGISTRY")
fi

if [[ -n ${SCANNER_DBIMAGE_NAME:-} ]]; then
    settings+=(--set scanner.dbImage.name="$SCANNER_DBIMAGE_NAME")
fi

if [[ -n ${SCANNER_DBIMAGE_TAG:-} ]]; then
    settings+=(--set scanner.dbImage.tag="$SCANNER_DBIMAGE_TAG")
fi
    
echo "${settings[@]}"



helm install -n stackrox stackrox-central-services --create-namespace rhacs/central-services \
    "${settings[@]}"

#helm install -n stackrox stackrox-central-services --create-namespace rhacs/central-services \
#  --set central.exposure.route.enabled=true \
#  --set central.adminPassword.value="$admin_password" \
#  --set imagePullSecrets.username="$DOCKER_USERNAME" \
#  --set imagePullSecrets.password="$DOCKER_PASSWORD" \
#  --set enableOpenShiftMonitoring=true \
#  --set central.exposeMonitoring=true \
#  --set central.image.name=main \
#  --set central.image.tag=3.68.x-198-g16f3ea3c01-dirty \
#  --set scanner.dbImage.registry=docker.io/stackrox \
#  --set scanner.dbImage.name=scanner-db \
#  --set scanner.dbImage.tag=2.22.0-15-gcc3102a65d
#  #--set central.image.tag=3.68.x-196-gc531c2675f

