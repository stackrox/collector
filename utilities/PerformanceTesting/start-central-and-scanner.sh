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
