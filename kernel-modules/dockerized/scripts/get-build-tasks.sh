#!/usr/bin/env bash

set -euo pipefail

# all-build-tasks will contain all potentially possible build tasks, i.e., the cross
# product between the set of kernel versions and the set of module versions
echo > "/all-build-tasks"

echo > "/non-blocklisted-build-tasks"

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

    [ -f "/kernel-modules/${version}/${driver}" ] || [ -f "/kernel-modules/${version}/${unavailable}" ]
}

process_driver() {
    local kernel_version="$1"
    local module_dir="$2"

    if ! driver_is_cached "$kernel_version" "$module_dir" "mod"; then
        append_task "$kernel_version" "$module_dir" "mod" /all-build-tasks
    fi

    if [[ -d "${module_dir}/bpf" ]] && ! driver_is_cached "$kernel_version" "$module_dir" "bpf"; then
        append_task "$kernel_version" "$module_dir" "bpf" /all-build-tasks
    fi
}

shopt -s nullglob
for module_dir in /kobuild-tmp/versions-src/*/; do
    if [[ "${USE_KERNELS_FILE,,}" == "true" ]]; then
        while IFS="" read -r kernel_version || [[ -n "$kernel_version" ]]; do
            process_driver "$kernel_version" "$module_dir"
        done < /KERNEL_VERSIONS
    else
        for bundle in /bundles/bundle-*.tgz; do
            kernel_version="${bundle%".tgz"}"
            kernel_version="${kernel_version#"/bundles/bundle-"}"

            process_driver "$kernel_version" "$module_dir"
        done
    fi
done

# non-blocklisted-build-tasks is populated from the BLOCKLIST file to exclude build tasks which would fail.
/scripts/apply-blocklist.py /scripts/BLOCKLIST /all-build-tasks > /non-blocklisted-build-tasks
/scripts/apply-blocklist.py /scripts/dockerized/BLOCKLIST /non-blocklisted-build-tasks > /build-tasks
