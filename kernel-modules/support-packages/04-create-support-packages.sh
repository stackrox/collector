#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

source "${SCRIPT_DIR}/utils.sh"

die() {
    echo >&2 "$@"
    exit 1
}

generate_checksum() (
    directory=$1
    file=$2
    cd "${directory}"
    sha256sum "${file}" > "${file}.sha256"
)

compress_files() (
    package_root=$1
    output_file=$2

    cd "${package_root}"
    zip -r "${output_file}" .
)

LICENSE_FILE="$1"
COLLECTOR_MODULES_BUCKET="$2"
MD_DIR="$3"
OUT_DIR="$4"
DOWNSTREAM_MODULES_BUCKET="$5"

[[ -n "$LICENSE_FILE" && -n "$MD_DIR" && -n "$OUT_DIR" ]] || die "Usage: $0 <license-file> <metadata directory> <output directory>"
[[ -d "$MD_DIR" ]] || die "Metadata directory $MD_DIR does not exist or is not a directory."

[[ -n "${COLLECTOR_MODULES_BUCKET:-}" ]] || die "Must specify a COLLECTOR_MODULES_BUCKET"

mkdir -p "${OUT_DIR}" || die "Failed to create output directory '${OUT_DIR}'"

for mod_ver_dir in "${MD_DIR}/module-versions"/*; do
    mod_ver="$(basename "$mod_ver_dir")"

    package_root="$(mktemp -d)"
    probe_dir="${package_root}/kernel-modules/${mod_ver}"
    mkdir -p "$probe_dir"
    # For now we create *full* kernel support packages, not only deltas, in order to
    # support the slim collector use-case.
    # Remains to be clarified; we might provide more fine granular download options in the future.
    gsutil -m cp "${COLLECTOR_MODULES_BUCKET}/${mod_ver}/*.gz" "$probe_dir"
    if use_downstream "$mod_ver"; then
        gsutil -m cp "${DOWNSTREAM_MODULES_BUCKET}/${mod_ver}/*.gz" "$probe_dir"
    fi

    package_out_dir="${OUT_DIR}/${mod_ver}"
    mkdir -p "$package_out_dir"
    filename="support-pkg-${mod_ver}-$(date '+%Y%m%d%H%M%S').zip"
    latest_filename="support-pkg-${mod_ver}-latest.zip"

    cp "${LICENSE_FILE}" "${probe_dir}"/LICENSE

    compress_files "$package_root" "${package_out_dir}/${filename}"
    generate_checksum "${package_out_dir}" "${filename}"

    # Export the latest build filename
    echo "${filename}" > "${package_out_dir}/latest"

    cp "${package_out_dir}/${filename}" "${package_out_dir}/${latest_filename}"
    generate_checksum "${package_out_dir}" "${latest_filename}"
    rm -rf "$package_root" || true
done
