#!/usr/bin/env bash

set -exuo pipefail

WORK_BRANCH="${1:-master}"
BUILD_LEGACY="${2:-false}"

mkdir -p /versions/{released-collectors,released-modules}
mkdir -p /kobuild-tmp/versions-src

get_driver_relative_path() (
    if git config --list -f /collector/.gitmodules --name-only | grep -q "falcosecurity-libs"; then
        echo "falcosecurity-libs"
    else
        echo "sysdig/src"
    fi
)

get_module_version() (
    if [[ -f /collector/kernel-modules/MODULE_VERSION ]]; then
        cat /collector/kernel-modules/MODULE_VERSION
    else
        echo ""
    fi
)

apply_patches() (
    for version_dir in /kobuild-tmp/versions-src/*; do
        version="${version_dir#"/kobuild-tmp/versions-src/"}"
        echo "Version directory: $version_dir"
        echo "Version: $version"
        if [[ -f "/collector/kernel-modules/patches/${version}.patch" ]]; then
            echo "Applying patch for module version ${version} ..."
            patch -p1 -d "/kobuild-tmp/versions-src/${version}" < "/collector/kernel-modules/patches/${version}.patch"
        fi
    done
)

checkout_branch() (
    local branch="$1"

    if [[ -z "$branch" ]]; then
        echo "Cannot checkout empty branch"
        return
    fi

    git -C /collector submodule deinit "$(get_driver_relative_path)"
    git -C /collector checkout "$branch"
    git -C /collector checkout -- .
    git -C /collector clean -xdf
    git -C /collector submodule update --init "$(get_driver_relative_path)"
)

if [[ "${CHECKOUT_BEFORE_PATCHING,,}" == "true" ]]; then
    # Prepare the sources for the work branch
    checkout_branch "$WORK_BRANCH"
fi

DRIVER_DIR="/collector/$(get_driver_relative_path)" \
SCRATCH_DIR="/scratch" \
OUTPUT_DIR="/kobuild-tmp/versions-src" \
LEGACY_DIR="/collector" \
M_VERSION="$(get_module_version)" \
    /scripts/prepare-src.sh

legacy="$(echo "$BUILD_LEGACY" | tr '[:upper:]' '[:lower:]')"
if [[ "$legacy" == "false" ]]; then
    # We are not building legacy probes, patch and move on
    apply_patches
    exit 0
fi

echo "Building legacy drivers"
# Loop through collector versions and create the required patched sources.
while IFS='' read -r line || [[ -n "$line" ]]; do
    [[ -n "$line" ]] || continue

    collector_ref="$line"
    echo "Preparing module source archive for collector version ${collector_ref}"

    checkout_branch "$collector_ref"

    DRIVER_DIR="/collector/$(get_driver_relative_path)" \
    SCRATCH_DIR="/scratch" \
    OUTPUT_DIR="/kobuild-tmp/versions-src" \
    LEGACY_DIR="/collector" \
    M_VERSION="$(get_module_version)" \
        /scripts/prepare-src.sh

done < <(grep -v '^#' < /collector/RELEASED_VERSIONS | awk -F'#' '{print $1}' | awk 'NF==2 {print $1}' | sort | uniq)

apply_patches

# Leave the collector repo as clean as possible
checkout_branch "$WORK_BRANCH"
