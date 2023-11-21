#!/usr/bin/env bash

set -euxo pipefail

verify_downloaded_file() {
  file=$1
  [[ -s "$file" ]] || {
    echo >&2 "Downloaded ${file} is empty or does not exist."
    echo >&2 "Please make sure that download URL begins with https://cdn.stackrox.io"
    exit 2
  }
}

main() {
  MODULE_VERSION="$(cat kernel-modules/MODULE_VERSION)"
  SUPPORT_PKG_VERSION="20231116131843"

  # Make sure to update this URL when releasing the new version of ACS.
  # Get the most current link at https://cdn.stackrox.io/collector/support-packages/index.html
  # DO NOT use "stable link", i.e. the URL MUST NOT end with "-latest.zip". This is done to avoid uploading 1.5GB
  # to the lookaside cache (which is short on free disk space) each time we run `build-pipeline`.
  # Be sure to replace https://install.stackrox.io with https://cdn.stackrox.io because the former one redirects (status
  # 302) to  the latter one. I'm NOT enabling curl redirects because this can potentially take us to untrusted locations
  # and I do not want to risk downloading kernel drivers from untrusted location.
  support_pkg="https://cdn.stackrox.io/collector/support-packages/x86_64/${MODULE_VERSION}/support-pkg-${MODULE_VERSION}-${SUPPORT_PKG_VERSION}.zip"

  zip_file="$(basename "${support_pkg}")"

  # This downloads the support package with all collector kernel drivers (probes) built upstream.
  # Eventually this needs to go away and we should build kernel drivers downstream.
  # See https://stack-rox.atlassian.net/browse/RS-230
  #
  # Utilizing curl with "--fail --location --max-redirs 0" ensures failure
  # on redirect attempt because we don't expect any.
  curl --fail --location --max-redirs 0 --output "${zip_file}" "${support_pkg}"
  curl --fail --location --max-redirs 0 --output "${zip_file}.sha256" "${support_pkg}.sha256"

  verify_downloaded_file "$zip_file"
  verify_downloaded_file "${zip_file}.sha256"

  sha256sum -c "${zip_file}.sha256"

  # Rename the support package so the docker build can find it in the same place every build.
  mv "$zip_file" "support-pkg.zip"
}

main
