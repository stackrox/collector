#!/usr/bin/env bash
set -exuo pipefail

collector_repos=(
    "collector"
)

container_build_dir="${SOURCE_ROOT}/kernel-modules/container"
layer_count="$("${container_build_dir}/partition-probes.py" -1 "$MAX_LAYER_MB" "${container_build_dir}/kernel-modules" "-")"
for collector_repo in "${collector_repos[@]}"; do
    build_args=(
        --build-arg collector_repo="${DOCKER_REPO}/${collector_repo}"
        --build-arg collector_version="$COLLECTOR_VERSION"
        --build-arg module_version="$(cat "${SOURCE_ROOT}/kernel-modules/MODULE_VERSION")"
        --build-arg max_layer_size="$MAX_LAYER_MB"
        --build-arg max_layer_depth="$layer_count"
    )
    docker build \
        --target="probe-layer-${layer_count}" \
        -t "${DOCKER_REPO}/${collector_repo}:${COLLECTOR_VERSION}-cpaas" \
        -t "${DOCKER_REPO}/${collector_repo}:${COLLECTOR_VERSION}-cpaas-latest" \
        -t "${QUAY_REPO}/${collector_repo}:${COLLECTOR_VERSION}-cpaas" \
        -t "${QUAY_REPO}/${collector_repo}:${COLLECTOR_VERSION}-cpaas-latest" \
        -t "${PUBLIC_REPO}/${collector_repo}:${COLLECTOR_VERSION}-cpaas" \
        -t "${PUBLIC_REPO}/${collector_repo}:${COLLECTOR_VERSION}-cpaas-latest" \
        "${build_args[@]}" \
        "${container_build_dir}"
done
