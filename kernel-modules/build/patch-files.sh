#!/usr/bin/env bash

set -euo pipefail

WORK_BRANCH="${1:-master}"
BUILD_LEGACY="${2:-false}"
SRC_DIR="${3:-/collector}"
PREPARE_SRC_REL="${4:-}"
OUT_DIR="${5:-}"

PREPARE_SRC_SH=/scripts/prepare-src.sh
if [[ -n "${PREPARE_SRC_REL}" ]]; then
    PREPARE_SRC_SH="${SRC_DIR}/${PREPARE_SRC_REL}"
fi

mkdir -p "${OUT_DIR}/versions/{released-collectors,released-modules}"
mkdir -p "${OUT_DIR}/kobuild-tmp/versions-src"

# The only driver currently support is falco, but I'm keeping this function
# in case this changes in the future.
get_driver_relative_path() (
    echo "falcosecurity-libs"
)

get_module_version() (
    cat "${SRC_DIR}/kernel-modules/MODULE_VERSION"
)

checkout_branch() (
    local branch="$1"

    if [[ -z "$branch" ]]; then
        echo "Cannot checkout empty branch"
        return 1
    fi

    git -C "${SRC_DIR}" submodule deinit "$(get_driver_relative_path)"
    git -C "${SRC_DIR}" checkout "$branch"
    git -C "${SRC_DIR}" checkout -- .
    git -C "${SRC_DIR}" clean -xdf

    local driver_relative_path
    driver_relative_path="$(get_driver_relative_path)"

    git -C "${SRC_DIR}" submodule update --init "${driver_relative_path}"

    return 0
)

if [[ "${CHECKOUT_BEFORE_PATCHING,,}" == "true" ]]; then
    # Prepare the sources for the work branch
    checkout_branch "$WORK_BRANCH"
fi

DRIVER_DIR="${SRC_DIR}/$(get_driver_relative_path)" \
SCRATCH_DIR="${OUT_DIR}/scratch" \
OUTPUT_DIR="${OUT_DIR}/kobuild-tmp/versions-src" \
LEGACY_DIR="${SRC_DIR}" \
M_VERSION="$(get_module_version)" \
    "${PREPARE_SRC_SH}"

legacy="$(echo "$BUILD_LEGACY" | tr '[:upper:]' '[:lower:]')"
if [[ "$legacy" == "false" ]]; then
    # We are not building legacy probes, move on
    exit 0
fi

echo "Building legacy drivers"
# Loop through collector versions and create the required patched sources.
while IFS='' read -r line || [[ -n "$line" ]]; do
    [[ -n "$line" ]] || continue

    collector_ref="$line"
    echo "Preparing module source archive for collector version ${collector_ref}"

    if ! checkout_branch "$collector_ref"; then
        continue
    fi

    DRIVER_DIR="${SRC_DIR}/$(get_driver_relative_path)" \
    SCRATCH_DIR="${OUT_DIR}/scratch" \
    OUTPUT_DIR="${OUT_DIR}/kobuild-tmp/versions-src" \
    LEGACY_DIR="${SRC_DIR}" \
    M_VERSION="$(get_module_version)" \
        "${PREPARE_SRC_SH}"

done < <(grep -v '^#' < "${SRC_DIR}/RELEASED_VERSIONS" | awk -F'#' '{print $1}' | awk 'NF==2 {print $1}' | sort | uniq)

# Leave the collector repo as clean as possible
checkout_branch "$WORK_BRANCH"
