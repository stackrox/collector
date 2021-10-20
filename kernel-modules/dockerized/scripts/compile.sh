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

	if [[ $type == "mod" ]]; then
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
	compile "$@" "mod"
)

compile_bpf() (
	compile "$@" "bpf"
)

for file in /bundles/bundle-*.tgz; do
	# Get the output directory and kernel version
	KERNEL_SRC="${file%".tgz"}"
	KERNEL_VERSION="${KERNEL_SRC#"/bundles/bundle-"}"

	# Extract a kernel source bundle
	mkdir -p "$KERNEL_SRC"
	tar -xzf "$file" -C "$KERNEL_SRC"

	KERNEL_BUILD="${KERNEL_SRC}/$(cat "${KERNEL_SRC}/BUNDLE_BUILD_DIR")"

	for module in /kobuild-tmp/versions-src/*; do
		# TODO: apply blocklist to tasks

		compile_kmod "$module" "$KERNEL_SRC" "$KERNEL_BUILD" "$KERNEL_VERSION"
		compile_bpf "$module" "$KERNEL_SRC" "$KERNEL_BUILD" "$KERNEL_VERSION"
	done
done
