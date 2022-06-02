#!/usr/bin/env bash
set -eo pipefail

mkdir -p ~/kobuild-tmp/versions-src
DRIVER_DIR="${SOURCE_ROOT}/falcosecurity-libs" \
    SCRATCH_DIR="${HOME}/scratch" \
    OUTPUT_DIR="${HOME}/kobuild-tmp/versions-src" \
    M_VERSION="$(cat "${SOURCE_ROOT}/kernel-modules/MODULE_VERSION")" \
    LEGACY_DIR="${SOURCE_ROOT}" \
    "${SOURCE_ROOT}/kernel-modules/build/prepare-src"

versions=(~/kobuild-tmp/versions-src/*)
[[ "${#versions[@]}" == 1 ]] || {
    echo >&2 "Expected a unique kernel module source version, got:"
    printf >&2 '- %s\n' "${versions[@]}"
    exit 1
}

version="$(basename "${versions[0]}" .tgz)"
MODULE_VERSION="$version"

echo "export MODULE_VERSION=""${MODULE_VERSION}" >> "${WORKSPACE_ROOT}/shared-env"
echo "export MODULE_VERSION=""${MODULE_VERSION}" >> "$BASH_ENV"
echo "Building modules for module version $version"
