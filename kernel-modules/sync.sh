#!/usr/bin/env bash

if [[ -z "$2" ]]; then
    echo "Usage: $0 gs://bucket-address listing-files..." >&2
    exit 2
fi

set -eu
set -o pipefail

test -x "$(which curl)"
test -x "$(which gsutil)"

bucket="$1"
gsutil ls "${bucket}/" > /dev/null

tmpdir=$(mktemp -d)
cleanup() {
    rm -r "${tmpdir}"
}

trap cleanup EXIT

gsutil ls "${bucket}/**" > "${tmpdir}/existing"


download_coreos() {
    download "${1}/coreos_developer_container.bin.bz2" "${2}coreos_developer_container.bin.bz2"
    # todo: extract bundle.tgz if it's not present
}

download() {
    url="$1"
    path="$2"
    if grep "${path}" "${tmpdir}/existing" > /dev/null ; then return; fi
    file="${tmpdir}/$(basename "${url}")"
    echo -n "Downloading ${url}..."
    curl -fs -o "${file}" "${url}"
    gsutil cp "${file}" "${path}"
    # echo "${path}"
}

cat "${@:2}" | while read -r url ; do
    path="${bucket}/${url//\/\//\/}"
    if [[ "${url}" =~ core-os.net ]]; then
	download_coreos "${url}" "${path}"
    else
	download "${url}" "${path}"
    fi
done
exit 0
