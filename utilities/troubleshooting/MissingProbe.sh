#!/usr/bin/env bash
set -eo pipefail

# Some basic troubleshooting if a probe is missing
# Checks if the kernel version is in a list of supported kernels
# Checks if the probe exists for the module version used by the client
# Checks if the probe exists for any module version
# Checks if the bundle for the kernel exits

kernel=$1
probe_type=$2
module_version=$3
bucket=612dd2ee06b660e728292de9393e18c81a88f347ec52a39207c5166b5302b656

DIR=$(dirname "$0")

cd "$DIR/../.."

print_border() {
	echo ""
	echo "################"
	echo ""
}

check_if_branch_supports_kernel_version() {
	branch=$1

	print_border
	echo "Checking if kernel version is supported by branch $branch"
	echo ""

	kernel_version_file="kernel-modules/KERNEL_VERSIONS"
	
	nfound=$(git grep -w "$kernel" "$branch" -- "$kernel_version_file" | wc -l)
	if [[ "$nfound" == "0" ]]; then
	        echo "Kernel $kernel NOT found in $kernel_version_file in the $branch branch"
	elif [[ "$nfound" == "1" ]]; then
	        echo "Kernel $kernel FOUND in $kernel_version_file in the $branch branch"
	else [[ $nfound -gt 1 ]]
	        echo "There is a bug in the code for checking if the kernel is listed in the KERNEL_VERSION file"
	        exit 3
	fi

	print_border
}

check_if_probe_exists_for_module_version() {
	module_version=$1

	print_border
	echo "Checking if the probe exists for the module version"
	echo ""

	if [[ $probe_type == "mod" ]]; then
		probe_file=gs://collector-modules/$bucket/$module_version/collector-$kernel.ko.gz
	else
		probe_file=gs://collector-modules/$bucket/$module_version/collector-ebpf-$kernel.o.gz
	fi

	if ! gsutil ls "$probe_file"; then
		echo "The probe for $kernel does NOT exist for module version $module_version"
	else
		echo "The probe for $kernel DOES exist for module version $module_version"
	fi

	print_border
}

check_if_probe_exists_for_any_module_version() {

	print_border
	echo "Checking if probe exists for any module version"
	echo ""

	if [[ $probe_type == "mod" ]]; then
		probe_file="gs://collector-modules/$bucket/*/collector-$kernel.ko.gz"
	else
		probe_file="gs://collector-modules/$bucket/*/collector-ebpf-$kernel.o.gz"
	fi

	if ! gsutil ls "$probe_file"; then
		echo "A probe for $kernel does NOT exist in any module version"
	else
		echo "A probe for $kernel DOES exist in at lease one module version"
	fi
	
	print_border
}

check_if_bundle_exists() {
	print_border
	echo "Checking if bundle exists"
	echo ""

	bundle_file=gs://stackrox-kernel-bundles/bundle-$kernel.tgz

	if ! gsutil ls "$bundle_file"; then
		echo "The bundle for $kernel does NOT exist"
	else
		echo "The bundle for $kernel DOES exist"
	fi

	print_border
}

check_if_branch_supports_kernel_version master
check_if_probe_exists_for_module_version "$module_version"
check_if_probe_exists_for_any_module_version
check_if_bundle_exists
