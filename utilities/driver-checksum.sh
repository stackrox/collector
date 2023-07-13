#!/usr/bin/env bash

set -euo pipefail

function process_gcp_driver() {
    checksum="${1}"
    file_in_bucket="${2}"
    retval=0

    temp_dir="$(mktemp -d)"
    driver="${temp_dir}/driver"

    echo "Downloading ${file_in_bucket} to ${driver}"

    gsutil cp "${file_in_bucket}" "${driver}"

    if ! process_file_driver "${checksum}" "${driver}"; then
        retval=1
    fi

    rm -rf "${temp_dir}"
    return $retval
}

function process_file_driver() {
    retval=0

    temp_file=$(mktemp)
    echo "${1} ${2}" > "${temp_file}"

    if ! sha256sum -c "${temp_file}"; then
        echo >&2 "Checksum validation failed"
        retval=1
    fi

    rm -f "${temp_file}"
    return $retval
}

SOURCE=${1}
CHECKSUM=${2}
FILE=${3}

if ((${#CHECKSUM} != 64)); then
    echo >&2 "Invalid length for checksum '${#CHECKSUM}'"
    exit 1
fi

if [[ "${SOURCE}" == "gcp" ]]; then
    process_gcp_driver "${CHECKSUM}" "${FILE}"
elif [[ "${SOURCE}" == "file" ]]; then
    process_file_driver "${CHECKSUM}" "${FILE}"
else
    echo >&2 "Invalid source for lookup provided '${SOURCE}'"
    exit 1
fi
