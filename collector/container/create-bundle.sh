#!/usr/bin/env bash
# Creates a tgz bundle of all binary artifacts needed for collector-rhel

set -euo pipefail

die() {
    echo >&2 "$@"
    exit 1
}

INPUT_ROOT="$1"
OUTPUT_DIR="$2"

[[ -n "$INPUT_ROOT" && -n "$OUTPUT_DIR" ]] \
    || die "Usage: $0 <input-root> <module-archive> <output-dir>"
[[ -d "$INPUT_ROOT" ]] \
    || die "Input root directory doesn't exist or is not a directory."
[[ -d "$OUTPUT_DIR" ]] \
    || die "Output directory doesn't exist or is not a directory."

OUTPUT_BUNDLE="${OUTPUT_DIR}/bundle.tar.gz"

# Create tmp directory
bundle_root="$(mktemp -d)"
mkdir -p "${bundle_root}/usr/"{bin,lib64,local/bin,local/lib}
mkdir -p "${bundle_root}/kernel-modules"
chmod -R 755 "${bundle_root}"

# =============================================================================
# Copy scripts to image build context directory

mkdir -p "${OUTPUT_DIR}/scripts"
cp "${INPUT_ROOT}/scripts/bootstrap.sh"          "${OUTPUT_DIR}/scripts"
cp "${INPUT_ROOT}/scripts/collector-wrapper.sh" "${OUTPUT_DIR}/scripts"

# =============================================================================

# Add binaries and data files to be included in the Dockerfile here. This
# includes artifacts that would be otherwise downloaded or included via a COPY
# command in the Dockerfile.

cp -p "${INPUT_ROOT}/libs/libsinsp-wrapper.so" "${bundle_root}/usr/local/lib/libsinsp-wrapper.so"
cp -p "${INPUT_ROOT}/scripts/bootstrap.sh" "${bundle_root}/bootstrap.sh"
cp -p "${INPUT_ROOT}/scripts/collector-wrapper.sh" "${bundle_root}/usr/local/bin/"
cp -r "${INPUT_ROOT}/THIRD_PARTY_NOTICES" "${bundle_root}/THIRD_PARTY_NOTICES"
cp -r "${INPUT_ROOT}/LICENSE-kernel-modules.txt" "${bundle_root}/kernel-modules/LICENSE"
cp -p "${INPUT_ROOT}/bin/collector" "${bundle_root}/usr/local/bin/collector"

# =============================================================================

# Files should have owner/group equal to root:root
if tar --version | grep -q "gnu"; then
    tar_chown_args=("--owner=root:0" "--group=root:0")
else
    tar_chown_args=("--disable-copyfile")
fi

# Create output bundle of all files in $bundle_root
tar cz "${tar_chown_args[@]}" --file "$OUTPUT_BUNDLE" --directory "${bundle_root}" .

# Clean up after success
rm -r "${bundle_root}"
