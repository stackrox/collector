#!/usr/bin/env bash

set -exuo pipefail

# This script handles decompressing the bundles tarballs and setting environment
# variables needed by the kernel driver compilation script.

KERNEL_SRC_DIR=""

# shellcheck source=/dev/null
source "/scripts/build-kos"

download_bundle() {
    local kernel_version="$1"

    if [[ -f "/bundles/${CURRENT_SHARD}/bundle-${kernel_version}.tgz" ]]; then
        return
    fi

    rm -rf "/bundles/${CURRENT_SHARD}"/* 2> /dev/null || true
    mkdir -p "/bundles/${CURRENT_SHARD}"
    gsutil -m cp "gs://collector-kernel-bundles-public/bundle-${kernel_version}.tgz" "/bundles/${CURRENT_SHARD}/bundle-${kernel_version}.tgz"
}

osci_handle_bundle() {
    local kernel_version="$1"

    download_bundle "$kernel_version"

    KERNEL_SRC_DIR="/scratch/kernel-src/${CURRENT_SHARD}/${kernel_version}"

    if [[ -d "$KERNEL_SRC_DIR" ]]; then
        return
    fi

    rm -rf "/scratch/kernel-src/${CURRENT_SHARD}"/* 2> /dev/null || true
    mkdir -p "$KERNEL_SRC_DIR"
    tar -C "$KERNEL_SRC_DIR" -xzf "/bundles/${CURRENT_SHARD}/bundle-${kernel_version}.tgz"
}

handle_bundle() {
    local kernel_version="$1"

    KERNEL_SRC_DIR="/scratch/kernel-src/${kernel_version}"

    if [[ -d "$KERNEL_SRC_DIR" ]]; then
        return
    fi

    rm -rf /scratch/kernel-src/* 2> /dev/null || true
    mkdir -p "$KERNEL_SRC_DIR"
    tar -C "$KERNEL_SRC_DIR" -xzf "/bundles/bundle-${kernel_version}.tgz"
}

extract_bundle() {
    local kernel_version="$1"

    export KERNEL_SRC_DIR
    if ((OSCI_RUN)); then
        osci_handle_bundle "$kernel_version"
    else
        handle_bundle "$kernel_version"
    fi

    [[ -f "${KERNEL_SRC_DIR}/BUNDLE_BUILD_DIR" ]] || {
        echo "No BUNDLE_BUILD_DIR entry found in kernel source bundle!"
        return 1
    }

    [[ -f "${KERNEL_SRC_DIR}/BUNDLE_UNAME" ]] || {
        echo "No BUNDLE_UNAME entry found in kernel source bundle!"
        return 1
    }

    return 0
}

export_env_variables() {
    export bundle_uname
    export bundle_version
    export bundle_major
    export bundle_distro
    export kernel_build_dir

    bundle_uname="$(cat "${KERNEL_SRC_DIR}/BUNDLE_UNAME")"
    bundle_version="$(cat "${KERNEL_SRC_DIR}/BUNDLE_VERSION")"
    bundle_major="$(cat "${KERNEL_SRC_DIR}/BUNDLE_MAJOR")"
    bundle_distro="$(cat "${KERNEL_SRC_DIR}/BUNDLE_DISTRO")"
    kernel_build_dir="${KERNEL_SRC_DIR}/$(cat "${KERNEL_SRC_DIR}/BUNDLE_BUILD_DIR")"
}

clean_env_variables() {
    unset bundle_uname
    unset bundle_version
    unset bundle_major
    unset bundle_distro
    unset kernel_build_dir

    unset KERNEL_SRC_DIR
}

build() {
    while read -r -a line || [[ "${#line[@]}" -gt 0 ]]; do
        local kernel_version="${line[0]}"
        local module_version="${line[1]}"
        local probe_type="${line[2]}"

        if ! extract_bundle "${kernel_version}"; then
            echo >&2 "Failed to extract kernel bundle for version '${kernel_version}'"
            exit 1
        fi

        export_env_variables

        failure_output_file="${FAILURE_DIR}/${kernel_version}/${module_version}/${probe_type}.log"
        mkdir -p "$(dirname "$failure_output_file")"
        if ! build_ko "$kernel_version" "$module_version" "${probe_type}" 2> >(tee "$failure_output_file" >&2); then
            echo >&2 "Failed to build ${probe_type} probe version ${module_version} for kernel ${kernel_version}"
        else
            rm -f "$failure_output_file"
        fi

        clean_env_variables
    done
}

DOCKERIZED=${DOCKERIZED:-0}
OSCI_RUN=${OSCI_RUN:-0}

export DOCKERIZED
export OSCI_RUN

if ((DOCKERIZED)); then
    FAILURE_DIR="/FAILURES"

    if ((OSCI_RUN)); then
        export MODULE_BASE_DIR="/built-drivers"
    else
        export MODULE_BASE_DIR="/kernel-modules"
    fi

    mkdir -p "$FAILURE_DIR"
    mkdir -p "$MODULE_BASE_DIR"
else
    FAILURE_DIR="/output/FAILURES"
    export MODULE_BASE_DIR="/output"
fi

# The input is in the form <kernel-version> <module-version>. Sort it to make sure that we first build all modules
# for a given kernel version before advancing to the next kernel version.
sort | uniq | build

# Remove empty directories
if ((DOCKERIZED)); then
    find "$FAILURE_DIR" -mindepth 1 -type d -empty -delete
fi
