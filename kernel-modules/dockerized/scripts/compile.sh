#!/usr/bin/env bash

set -euo pipefail

# Create the output directory in case it doesn't exist already
mkdir -p /kernel-modules

compile() (
	local collector_src="$1"
	local kernel_src_dir="$2"
	local kernel_build_dir="$3"
	local kversion="$4"
	local type="$5"

	collector_version="${collector_src#"/kobuild-tmp/versions-src/"}"
	mkdir -p /kernel-modules/"$collector_version"

	if [[ "$type" == "mod" ]]; then
		src_dir="$collector_src"
		object="$src_dir"/collector.ko
		compressed=/kernel-modules/"$collector_version"/collector-"$kversion".ko.gz
		echo "Compiling kernel module for '$kversion'"
	else
		src_dir="$collector_src"/bpf
		object="$src_dir"/probe.o
		compressed=/kernel-modules/"$collector_version"/collector-ebpf-"$kversion".o.gz
		echo "Compiling eBPF probe for '$kversion'"
	fi

	KERNELDIR="$kernel_src_dir" BUILD_ROOT="$kernel_build_dir" make -C "$src_dir" all

	# Copy the kernel module for safe keeping
	gzip -c "$object" > "$compressed"

	# Clean the compilation directory once done
	KERNELDIR="$kernel_src_dir" BUILD_ROOT="$kernel_build_dir" make -C "$src_dir" clean
)

compile_kmod() (
	local collector_src="$1"
	local kernel_src_dir="$2"
	local kernel_build_dir="$3"
	local kversion="$4"

	# Attempt to run modpost
	if ! "${kernel_src_dir}/scripts/mod/modpost"; then
		echo "Failed to run kbuild tools, skipping module for ${kversion}"
		return 0
	fi

	compile "$@" "mod"
)

compile_bpf() (
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

	local rhel7_kernel_with_ebpf=false
	if (( bundle_version == 3 && bundle_major >= 10 )); then
		if [[ "$bundle_distro" == "redhat" ]]; then
			rhel_build_id="$(echo "$bundle_uname" | awk -F'[-.]' '{ print $4 }')"
			if (( rhel_build_id >= 957 )); then
				echo "Kernel ${bundle_uname} has backported eBPF support"
				rhel7_kernel_with_ebpf=true
			fi
		fi
	fi

	if [[ ! -d "${collector_src}/bpf" ]]; then
		echo "Module version does not support eBPF probe building, skipping ..."
		return 0
	fi

	[[ -n "$bundle_version" && -n "$bundle_major" ]] || {
		echo >&2 "Bundle does not contain major/minor version information!"
		return 1
	}

	# Check if this module version supports RHEL 7.6 with backported eBPF support
	if [[ "$rhel7_kernel_with_ebpf" == true ]]; then
		if ! grep -qRIs "SUPPORTS_RHEL76_EBPF" "${collector_src}/bpf/quirks.h"; then
			echo "Module version ${collector_src#"/kobuild-tmp/versions-src/"} does not support eBPF on RHEL 7"
			return 0
		fi

	# Check kernel version is at least 4.14 (unless RHEL 7.6 kernel detected)
	elif (( bundle_version < 4 || (bundle_version == 4 && bundle_major < 14) )); then
		echo "Kernel version ${kversion} does not support eBPF probe building, skipping ..."
		return 0
	fi

	compile "$@" "bpf"
)

shopt -s nullglob
for file in /bundles/bundle-*.tgz; do
	# Get the output directory and kernel version
	KERNEL_BUILD="${file%".tgz"}"
	KERNEL_VERSION="${KERNEL_BUILD#"/bundles/bundle-"}"

	# Extract a kernel source bundle
	mkdir -p "$KERNEL_BUILD"
	tar -xzf "$file" -C "$KERNEL_BUILD"

	KERNEL_SRC="${KERNEL_BUILD}/$(cat "${KERNEL_BUILD}/BUNDLE_BUILD_DIR")"

	for module in /kobuild-tmp/versions-src/*; do
		# TODO: apply blocklist to tasks

		if ! compile_kmod "$module" "$KERNEL_SRC" "$KERNEL_BUILD" "$KERNEL_VERSION"; then
			echo "Failed to build kernel driver for ${KERNEL_VERSION}"
		fi
		if ! compile_bpf "$module" "$KERNEL_SRC" "$KERNEL_BUILD" "$KERNEL_VERSION"; then
			echo "Failed to build eBPF probe for ${KERNEL_VERSION}"
		fi
	done
done
