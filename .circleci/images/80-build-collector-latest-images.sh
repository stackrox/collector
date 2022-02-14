#!/usr/bin/env bash
set -eo pipefail

BRANCH=$1
TAG=$2

collector_repos=(
    "collector"
)

# On PR branches, only build slim images
if [[ "$BRANCH" != "master" && -z "$TAG" && "${BUILD_FULL_IMAGES,,}" == "false" ]]; then
    echo "Not building full images, re-tagging base images..."
    for collector_repo in "${collector_repos[@]}"; do
        docker_full_repo="${DOCKER_REPO}/${collector_repo}:${COLLECTOR_VERSION}"
        quay_full_repo="${QUAY_REPO}/${collector_repo}:${COLLECTOR_VERSION}"
        docker tag "${docker_full_repo}-base" "${docker_full_repo}"
        docker tag "${docker_full_repo}-base" "${docker_full_repo}-latest"
        docker tag "${quay_full_repo}-base" "${quay_full_repo}"
        docker tag "${quay_full_repo}-base" "${quay_full_repo}-latest"
    done
    exit 0
fi

container_build_dir="${SOURCE_ROOT}/kernel-modules/container"
layer_count="$("${container_build_dir}/partition-probes.py" -1 "$MAX_LAYER_MB" "${container_build_dir}/kernel-modules" "-")"
for collector_repo in "${collector_repos[@]}"; do
    build_args=(
        --build-arg collector_repo="${DOCKER_REPO}/${collector_repo}"
        --build-arg collector_version="$COLLECTOR_VERSION"
        --build-arg module_version="$MODULE_VERSION"
        --build-arg max_layer_size="$MAX_LAYER_MB"
        --build-arg max_layer_depth="$layer_count"
        --build-arg USE_VALGRIND="$BUILD_USE_VALGRIND"
        --build-arg ADDRESS_SANITIZER="$SANITIZER_TESTS"
    )
    docker build \
        --target="probe-layer-${layer_count}" \
        -t "${DOCKER_REPO}/${collector_repo}:${COLLECTOR_VERSION}" \
        -t "${DOCKER_REPO}/${collector_repo}:${COLLECTOR_VERSION}-latest" \
        -t "${QUAY_REPO}/${collector_repo}:${COLLECTOR_VERSION}" \
        -t "${QUAY_REPO}/${collector_repo}:${COLLECTOR_VERSION}-latest" \
        "${build_args[@]}" \
        "${container_build_dir}"
done
