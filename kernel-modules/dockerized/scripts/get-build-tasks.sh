#!/usr/bin/env bash

set -euo pipefail

# all-build-tasks will contain all potentially possible build tasks, i.e., the cross
# product between the set of kernel versions and the set of module versions
echo >"/all-build-tasks"

# redundant-build-tasks will contain all build tasks for which we already have the resulting
# module.
echo >"/redundant-build-tasks"

echo >"/non-blocklisted-build-tasks"
echo >"/global-non-blocklisted-build-tasks"

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
	if [[ "$driver_type" == "mod" ]]; then
		driver="collector-${kernel_version}.ko.gz"
	else
		driver="collector-ebpf-${kernel_version}.o.gz"
	fi

	[ -f "/kernel-modules/${version}/${driver}" ]
}

shopt -s nullglob
for module_dir in /kobuild-tmp/versions-src/*/; do
	if [[ "${USE_KERNELS_FILE,,}" == "true" ]]; then
		while IFS="" read -r kernel_version || [[ -n "$kernel_version" ]]; do
			append_task "$kernel_version" "$module_dir" "mod" /all-build-tasks

			# If the driver is cached, we prevent rebuilding it by adding it to '/redundant-build-tasks'
			if driver_is_cached "$kernel_version" "$module_dir" "mod"; then
				append_task "$kernel_version" "$module_dir" "mod" /redundant-build-tasks
			fi

			if [[ -d "${module_dir}/bpf" ]]; then
				append_task "$kernel_version" "$module_dir" "bpf" /all-build-tasks

				if driver_is_cached "$kernel_version" "$module_dir" "bpf"; then
					append_task "$kernel_version" "$module_dir" "bpf" /redundant-build-tasks
				fi
			fi
		done < /KERNEL_VERSIONS
	else
		for bundle in /bundles/bundle-*.tgz; do
			kernel_version="${bundle%".tgz"}"
			kernel_version="${kernel_version#"/bundles/bundle-"}"

			# If the driver is cached, don't add it to the task file
			if ! driver_is_cached "$kernel_version" "$module_dir" "mod"; then
				append_task "$kernel_version" "$module_dir" "mod" /all-build-tasks
			fi

			if [[ -d "${module_dir}/bpf" ]] && ! driver_is_cached "$kernel_version" "$module_dir" "bpf"; then
				append_task "$kernel_version" "$module_dir" "bpf" /all-build-tasks
			fi
		done
	fi
done

# blocklisted-build-tasks is populated from the BLOCKLIST file to exclude build tasks which would fail.
/scripts/apply-blocklist.py /scripts/BLOCKLIST /all-build-tasks >/global-non-blocklisted-build-tasks
/scripts/apply-blocklist.py /scripts/dockerized/BLOCKLIST /global-non-blocklisted-build-tasks >/non-blocklisted-build-tasks

# Create the set of build tasks as the contents of `all-build-tasks` minus the redundant and blocklisted
# build tasks.
sort /non-blocklisted-build-tasks /redundant-build-tasks | awk NF | uniq -u >/build-tasks
