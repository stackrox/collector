#!/usr/bin/env bash
set -eo pipefail

# Some basic troubleshooting if a probe is missing
# Checks if the kernel version is in a list of supported kernels
# Checks if the probe exists for the module version used by the client
# Checks if the probe exists for any module version
# Checks if the bundle for the kernel exits

kernel=$1
module_version=$2
bucket=${3:-612dd2ee06b660e728292de9393e18c81a88f347ec52a39207c5166b5302b656}
branch=${4:-master}

DIR=$(dirname "$0")

cd "$DIR/../.."

print_border() {
    echo ""
    echo "################"
    echo ""
}

print_header() {
    print_border
    echo "$1"
    echo ""
}

get_probe_file() {
    local_probe_type="$1"
    local_bucket="$2"
    local_module_version="$3"
    local_kernel="$4"

    if [[ $local_probe_type == "mod" ]]; then
        probe_file="gs://collector-modules/$local_bucket/$local_module_version/collector-${local_kernel}.ko.gz"
    else
        probe_file="gs://collector-modules/$local_bucket/$local_module_version/collector-ebpf-${local_kernel}.o.gz"
    fi

    echo "$probe_file"
}

get_unavailable_probe_file() {
    local_probe_type="$1"
    local_bucket="$2"
    local_module_version="$3"
    local_kernel="$4"

    if [[ $local_probe_type == "mod" ]]; then
        probe_file="gs://collector-modules/$local_bucket/$local_module_version/.collector-${local_kernel}.ko.unavail"
    else
        probe_file="gs://collector-modules/$local_bucket/$local_module_version/.collector-ebpf-${local_kernel}.o.unavail"
    fi

    echo "$probe_file"
}

check_if_branch_supports_kernel_version() {
    local_branch=$1

    print_header "Checking if kernel version is supported by branch $local_branch"

    kernel_version_file="kernel-modules/KERNEL_VERSIONS"

    nfound=$({ git grep -w "$kernel" "$local_branch" -- "$kernel_version_file" || true; } | wc -l)
    if ((nfound == 0)); then
         echo "Kernel $kernel NOT found in $kernel_version_file in the $local_branch branch"
    elif ((nfound == 1)); then
         echo "Kernel $kernel FOUND in $kernel_version_file in the $local_branch branch"
    else
         echo >&2 "${nfound} matches found for ${kernel} in KERNEL_VERSIONS, this might be a bug in the troubleshooting script"
         exit 3
    fi

    print_border
}

check_if_gsutil_is_installed() {
    if ! command -v gsutil &> /dev/null; then
        "Error: gsutil is not installed. Please install it to use this script"
        exit 36
    fi
}

check_if_probe_exists_for_module_version() {
    module_version=$1
    local_probe_type=$2

    print_header "Checking if the $local_probe_type probe exists for the module version"

    probe_file="$(get_probe_file "$local_probe_type" "$bucket" "$module_version" "$kernel")"

    if ! gsutil ls "$probe_file"; then
        echo "The $local_probe_type probe for $kernel does NOT exist for module version $module_version"
    else
        echo "The $local_probe_type probe for $kernel DOES exist for module version $module_version"
    fi

    print_border
}

check_if_probe_is_marked_unavailable_for_module_version() {
    module_version=$1
    local_probe_type=$2

    print_header "Checking if the $local_probe_type probe is marked unavailable for the module version"

    probe_file="$(get_unavailable_probe_file "$local_probe_type" "$bucket" "$module_version" "$kernel")"

    if ! gsutil ls "$probe_file"; then
        echo "The $local_probe_type probe for $kernel is not marked as unavailable for $module_version. That does not mean that it is available"
    else
        echo "The $local_probe_type probe for $kernel is marked as unavailable for $module_version"
    fi

    print_border
}

check_if_probe_exists_for_any_module_version() {
    local_probe_type=$1

    print_header "Checking if the $local_probe_type probe exists for any module version"

    probe_file="$(get_probe_file "$local_probe_type" "$bucket" "*" "$kernel")"

    if ! gsutil ls "$probe_file"; then
        echo "A $local_probe_type probe for $kernel does NOT exist in any module version"
    else
        echo "A $local_probe_type probe for $kernel DOES exist in at least one module version"
    fi

    print_border
}

check_if_bundle_exists() {

    print_header "Checking if bundle exists"

    bundle_file="gs://stackrox-kernel-bundles/bundle-$kernel.tgz"

    if ! gsutil ls "$bundle_file"; then
        echo "The bundle for $kernel does NOT exist"
    else
        echo "The bundle for $kernel DOES exist"
    fi

    print_border
}

check_if_branch_supports_kernel_version "$branch"
check_if_gsutil_is_installed
check_if_bundle_exists

for probe_type in mod bpf
do	
    check_if_probe_exists_for_module_version "$module_version" "$probe_type"
    check_if_probe_is_marked_unavailable_for_module_version "$module_version" "$probe_type"
    check_if_probe_exists_for_any_module_version "$probe_type"
done
