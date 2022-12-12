#!/usr/bin/env bash

set -euo pipefail

KERNELS_FILE="${KERNELS_FILE:-/KERNEL_VERSIONS}"
OUTPUT_DIR="${OUTPUT_DIR:-}"
CACHE_DIR="${CACHE_DIR:-}"
BLOCKLIST_DIR="${BLOCKLIST_DIR:-/scripts}"
SCRIPTS_DIR="${SCRIPTS_DIR:-/scripts}"

# all-build-tasks will contain all potentially possible build tasks, i.e., the cross
# product between the set of kernel versions and the set of module versions
ALL_BUILD_TASKS="${OUTPUT_DIR}/all-build-tasks"
echo > "${ALL_BUILD_TASKS}"

NON_BLOCKLISTED_TASKS="${OUTPUT_DIR}/non-blocklisted-build-tasks"
echo > "${NON_BLOCKLISTED_TASKS}"

append_task() {
    local kernel_version="$1"
    local module_dir="$2"
    local driver_type="$3"
    local output_file="$4"

    local version
    version="$(basename "$module_dir")"

    printf "%s %s %s\n" "$kernel_version" "$version" "$driver_type" >> "$output_file"
}

driver_is_cached() {
    local kernel_version="$1"
    local module_dir="$2"
    local driver_type="$3"

    local version
    version="$(basename "$module_dir")"

    local driver
    local unavailable
    if [[ "$driver_type" == "mod" ]]; then
        driver="collector-${kernel_version}.ko.gz"
        unavailable=".collector-${kernel_version}.unavail"
    else
        driver="collector-ebpf-${kernel_version}.o.gz"
        unavailable=".collector-ebpf-${kernel_version}.unavail"
    fi

    [ -f "${CACHE_DIR}/kernel-modules/${version}/${driver}" ] || [ -f "${CACHE_DIR}/kernel-modules/${version}/${unavailable}" ]
}

process_driver() {
    local kernel_version="$1"
    local module_dir="$2"

    if ! driver_is_cached "$kernel_version" "$module_dir" "mod"; then
        append_task "$kernel_version" "$module_dir" "mod" "${ALL_BUILD_TASKS}"
    fi

    if [[ -d "${module_dir}/bpf" ]] && ! driver_is_cached "$kernel_version" "$module_dir" "bpf"; then
        append_task "$kernel_version" "$module_dir" "bpf" "${ALL_BUILD_TASKS}"
    fi
}

shopt -s nullglob
for module_dir in "${CACHE_DIR}/kobuild-tmp/versions-src"/*; do
    if [[ "${USE_KERNELS_FILE,,}" == "true" ]]; then
        moddir="${module_dir%".tgz"}"
        while IFS="" read -r kernel_version || [[ -n "$kernel_version" ]]; do
            process_driver "$kernel_version" "$moddir"
        done < "$KERNELS_FILE"
    else
        for bundle in "${CACHE_DIR}/bundles"/bundle-*.tgz; do
            kernel_version="${bundle%".tgz"}"
            kernel_version="${kernel_version#"${CACHE_DIR}/bundles/bundle-"}"

            process_driver "$kernel_version" "$module_dir"
        done
    fi
done

# non-blocklisted-build-tasks is populated from the BLOCKLIST file to exclude build tasks which would fail.
"${SCRIPTS_DIR}/apply-blocklist.py" "${BLOCKLIST_DIR}/BLOCKLIST" "${ALL_BUILD_TASKS}" > "${NON_BLOCKLISTED_TASKS}"
"${SCRIPTS_DIR}/apply-blocklist.py" "${BLOCKLIST_DIR}/dockerized/BLOCKLIST" "${NON_BLOCKLISTED_TASKS}" > "${OUTPUT_DIR}/build-tasks"
