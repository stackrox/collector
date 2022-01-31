#!/usr/bin/env bash

set -euo pipefail

# Create the output directory in case it doesn't exist already
mkdir -p /kernel-modules

create_unavailable_file() {
    local collector_version="$1"
    local kernel_version="$2"

    touch "/kernel-modules/${collector_version}/collector-ebpf-${kernel_version}.unavail"
}

compile_driver() {
    local collector_src="$1"
    local kernel_src_dir="$2"
    local kernel_build_dir="$3"
    local kversion="$4"
    local type="$5"

    collector_version="${collector_src#"/kobuild-tmp/versions-src/"}"

    if [[ "${type}" == "mod" ]]; then
        src_dir="${collector_src}"
        object="${src_dir}"/collector.ko
        compressed=/kernel-modules/"${collector_version}"/collector-"${kversion}".ko.gz
        echo "Compiling kernel module for '${kversion}'"
    else
        src_dir="$collector_src"/bpf
        object="$src_dir"/probe.o
        compressed=/kernel-modules/"${collector_version}"/collector-ebpf-"${kversion}".o.gz
        echo "Compiling eBPF probe for '${kversion}'"
    fi

    KERNELDIR="$kernel_src_dir" BUILD_ROOT="${kernel_build_dir}" make -C "${src_dir}" clean
    KERNELDIR="$kernel_src_dir" BUILD_ROOT="${kernel_build_dir}" make -C "${src_dir}" all

    if [[ "$type" == "mod" ]]; then
        # Extra steps required for kernel modules
        strip -g "$object"

        local bundle_uname
        bundle_uname="$(cat "${kernel_build_dir}/BUNDLE_UNAME")"

        ko_version="$(/sbin/modinfo "${object}" | grep vermagic | tr -s " " | cut -d " " -f 2)"
        if [[ "${ko_version}" != "${bundle_uname}" ]]; then
            echo "Corrupted probe, KO_VERSION=${ko_version}, BUNDLE_UNAME=${bundle_uname}" >&2
            return 1
        fi
    fi

    # Copy the kernel module for safe keeping
    if ! gzip -c "${object}" > "${compressed}"; then
        rm -f "${compressed}"
        echo >&2 "Failed to compress ${object}"
        return 1
    fi
}

compile_kmod() {
    local kernel_src_dir="$2"
    local kversion="$4"

    # Attempting to run modpost will fail if it requires glibc version newer than
    # available in this distro. We skip building such kernel drivers for now.
    if ! "${kernel_src_dir}/scripts/mod/modpost"; then
        echo >&2 "Failed to run kbuild tools, skipping module for ${kversion}"
        return 1
    fi

    compile_driver "$@" "mod"
}

compile_bpf() {
    local collector_src="$1"
    local kernel_src_dir="$2"
    local kernel_build_dir="$3"
    local kversion="$4"

    # Check eBPF is supported by the kernel version
    local bundle_uname bundle_version bundle_major bundle_distro
    bundle_uname="$(cat "${kernel_build_dir}/BUNDLE_UNAME")"
    bundle_version="$(cat "${kernel_build_dir}/BUNDLE_VERSION")"
    bundle_major="$(cat "${kernel_build_dir}/BUNDLE_MAJOR")"
    bundle_distro="$(cat "${kernel_build_dir}/BUNDLE_DISTRO")"

    collector_version="${collector_src#"/kobuild-tmp/versions-src/"}"

    if [[ ! -d "${collector_src}/bpf" ]]; then
        echo "Module version does not support eBPF probe building, skipping ..."
        create_unavailable_file "${collector_version}" "${kversion}"
        return 0
    fi

    [[ -n "${bundle_version}" && -n "${bundle_major}" ]] || {
        echo >&2 "Bundle does not contain major/minor version information!"
        return 1
    }

    local rhel7_kernel_with_ebpf=false
    if ((bundle_version == 3 && bundle_major >= 10)); then
        if [[ "${bundle_distro}" == "redhat" ]]; then
            local rhel_build_id
            rhel_build_id="$(echo "${bundle_uname}" | awk -F'[-.]' '{ print $4 }')"
            if ((rhel_build_id >= 957)); then
                echo "Kernel ${bundle_uname} has backported eBPF support"
                rhel7_kernel_with_ebpf=true
            fi
        fi
    fi

    # Check if this module version supports RHEL 7.6 with backported eBPF support
    if [[ "$rhel7_kernel_with_ebpf" == true ]]; then
        if ! grep -qRIs "SUPPORTS_RHEL76_EBPF" "${collector_src}/bpf/quirks.h"; then
            echo "Module version ${collector_src#"/kobuild-tmp/versions-src/"} does not support eBPF on RHEL 7, skipping ..."
            create_unavailable_file "${collector_version}" "${kversion}"
            return 0
        fi

    # Check kernel version is at least 4.14 (unless RHEL 7.6 kernel detected)
    elif ((bundle_version < 4 || (bundle_version == 4 && bundle_major < 14))); then
        echo "Kernel version ${kversion} does not support eBPF probe building, skipping ..."
        create_unavailable_file "${collector_version}" "${kversion}"
        return 0
    fi

    compile_driver "$@" "bpf"
}

compile() {
    mkdir -p /FAILURES/

    while read -r -a line || [[ "${#line[@]}" -gt 0 ]]; do
        local kernel_version="${line[0]}"
        local module_version="${line[1]}"
        local probe_type="${line[2]}"

        local kernel_build="/bundles/bundle-${kernel_version}"
        local kernel_bundle="${kernel_build}.tgz"
        local module="/kobuild-tmp/versions-src/${module_version}"

        if [[ ! -d "${kernel_build}" ]]; then
            mkdir -p "${kernel_build}"
            tar -xzf "${kernel_bundle}" -C "${kernel_build}"
        fi

        mkdir -p "/kernel-modules/${module_version}"

        local kernel_src
        kernel_src="${kernel_build}/$(cat "${kernel_build}/BUNDLE_BUILD_DIR")"

        local failure_output_file

        if [[ "${probe_type}" == "mod" ]]; then
            failure_output_file="/FAILURES/${module_version}/${kernel_version}/mod.log"
            mkdir -p "$(dirname "${failure_output_file}")"
            if ! compile_kmod "${module}" "${kernel_src}" "${kernel_build}" "${kernel_version}" 2> >(tee "${failure_output_file}" >&2); then
                echo >&2 "Failed to build kernel driver for ${kernel_version}"
            else
                rm "${failure_output_file}"
            fi
        elif [[ "${probe_type}" == "bpf" ]]; then
            failure_output_file="/FAILURES/${module_version}/${kernel_version}/bpf.log"
            mkdir -p "$(dirname "${failure_output_file}")"
            if ! compile_bpf "${module}" "${kernel_src}" "${kernel_build}" "${kernel_version}" 2> >(tee "${failure_output_file}" >&2); then
                echo >&2 "Failed to build eBPF probe for ${kernel_version}"
            else
                rm "${failure_output_file}"
            fi
        else
            echo >&2 "Unknown probe type ${probe_type}"
        fi

        rm -rf "${kernel_build}"
    done

    # Remove empty directories
    find /FAILURES/ -mindepth 1 -type d -empty -delete
}

# The input is in the form <kernel-version> <module-version> <driver-type>. Sort it to make sure that we first build all
# drivers for a given kernel version before advancing to the next kernel version.
sort | uniq | compile
