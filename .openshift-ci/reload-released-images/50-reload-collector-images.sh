#!/usr/bin/env bash
set -eo pipefail

reload_md_directory=$1
max_layer_mb=$2
docker_repo=$3
quay_repo=$4
build_use_valgrind=$5
sanitizer_tests=$6
branch=$7
tag=$8

for collector_dir in "${reload_md_directory}"/*; do
    collector_ver="$(basename "${collector_dir}")"
    mod_ver="$(< "${collector_dir}/module-version")"

    container_build_dir="${WORKSPACE_ROOT}/images/${mod_ver}/container"
    layer_count="$("${container_build_dir}/partition-probes.py" -1 "$max_layer_mb" "${container_build_dir}/kernel-modules" "-")"

    echo "Reloading ${layer_count} image layers for collector ${collector_ver} (${mod_ver})"

    dockerhub_repo="${docker_repo}/collector:${collector_ver}"
    dockerhub_base_image="${dockerhub_repo}-base"
    dockerhub_pr_image="${dockerhub_repo}-reload-latest"
    dockerhub_image="${dockerhub_repo}-latest"

    quay_base_repo="${quay_repo}/collector:${collector_ver}"
    quay_base_image="${quay_base_repo}-base"
    quay_pr_image="${quay_base_repo}-reload-latest"
    quay_image="${quay_base_repo}-latest"

    stackrox_io_image="collector.stackrox.io/collector:${collector_ver}-latest"

    image_list=(
        "${dockerhub_image}"
        "${dockerhub_base_image}"
        "${dockerhub_pr_image}"
        "${quay_image}"
        "${quay_base_image}"
        "${quay_pr_image}"
        "${stackrox_io_image}"
    )

    build_args=(
        --build-arg module_version="${mod_ver}"
        --build-arg collector_version="${collector_ver}"
        --build-arg collector_repo="${docker_repo}/collector"
        --build-arg max_layer_depth="${layer_count}"
        --build-arg max_layer_size="${MAX_LAYER_MB}"
        --build-arg USE_VALGRIND="${build_use_valgrind}"
        --build-arg ADDRESS_SANITIZER="${sanitizer_tests}"
    )

    docker build -q \
        --target="probe-layer-${layer_count}" \
        -t "${stackrox_io_image}" \
        -t "${dockerhub_image}" \
        -t "${dockerhub_pr_image}" \
        -t "${quay_pr_image}" \
        -t "${quay_image}" \
        "${build_args[@]}" \
        "${container_build_dir}"

    if [[ "$branch" != "master" && -z "$tag" ]]; then
        "${SOURCE_ROOT}"/scripts/push-as-manifest-list.sh "${dockerhub_pr_image}"
        "${SOURCE_ROOT}"/scripts/push-as-manifest-list.sh "${quay_pr_image}"
        image_list+=("${dockerhub_pr_image}-amd64" "${quay_pr_image}-amd64")
    else
        "${SOURCE_ROOT}"/scripts/push-as-manifest-list.sh "${dockerhub_image}"
        "${SOURCE_ROOT}"/scripts/push-as-manifest-list.sh "${quay_image}"
        "${SOURCE_ROOT}"/scripts/push-as-manifest-list.sh "${stackrox_io_image}"
        image_list+=("${dockerhub_image}-amd64" "${quay_image}-amd64" "${stackrox_io_image}-amd64")
    fi

    # clean-up to reduce disk usage
    docker image rm -f "${image_list[@]}"
    docker image prune --force
    docker images
done
