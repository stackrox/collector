#!/usr/bin/env bash

# A small script for quickly validating driver SHA256 checksums.
# See the 'usage' function or run './driver-checksum.sh -h' for a
# comprehensive explanation on how to use the script.

set -eo pipefail

function usage() {
    printf "usage: driver-checksum.sh source checksum driver\n"
    printf "\n"
    printf "    source\tSpecify if the drivers exists as a file or on gcp\n"
    printf "          \t  Values: gcp, file\n"
    printf "    checksum\tA SHA256 checksum to be compared against the driver's\n"
    printf "    driver\tThe path to the driver on GCP or the local fs\n"
    printf "\n"
    printf "    -h, --help\tThis help message\n"
}

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

args=()

while test $# -gt 0; do
    case "$1" in
        -h | --help)
            usage
            exit 0
            ;;
        -*)
            echo >&2 "Invalid argument '$1'"
            usage
            exit 1
            ;;
        *)
            args+=("$1")
            ;;
    esac
    shift
done

if ((${#args[@]} != 3)); then
    echo >&2 "Wrong number of arguments"
    usage
    exit 1
fi

SOURCE=${args[0]}
CHECKSUM=${args[1]}
FILE=${args[2]}

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
