#!/usr/bin/env bash

set -euo pipefail

compress_files() (
    package_root=$1
    output_file=$2

    cd "${package_root}"
    zip -r "${output_file}" .
)

PLATFORM="$1"
IMAGE="$2"
OUT_DIR="$3"

case "${PLATFORM}" in
    "x86_64") ;;
    "s390x") ;;
    "ppc64le") ;;
    *)
        echo >&2 "Invalid architecture ${PLATFORM}"
        exit 1
              ;;
esac

container="$(docker create --pull always --platform "linux/${PLATFORM}" "${IMAGE}")"

mkdir -p "${OUT_DIR}/${PLATFORM}"

docker cp "${container}:/support-packages" "${OUT_DIR}/${PLATFORM}"
docker rm "${container}"
