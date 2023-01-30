#!/usr/bin/env bash

set -euo pipefail

generate_checksum() (
    directory=$1
    file=$2
    cd "${directory}" || return
    sha256sum "${file}" > "${file}.sha256"
)

while read -r version original_file; do
    VERSION_PATH="${SUPPORT_PACKAGE_TMP_DIR}/${version}"
    latest_file="support-pkg-${version}-latest.zip"
    generate_checksum "${VERSION_PATH}" "${original_file}"

    echo "${original_file}" > "${VERSION_PATH}/latest"

    cp "${VERSION_PATH}/${original_file}" "${VERSION_PATH}/${latest_file}"
    generate_checksum "${VERSION_PATH}" "${latest_file}"
done < <(find "${SUPPORT_PACKAGE_TMP_DIR}" -name '*.zip' -type f \
    | awk '{split($0,array,"/"); print array[5] " " array[6]}')
