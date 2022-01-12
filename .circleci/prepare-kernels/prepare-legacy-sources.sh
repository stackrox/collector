#!/usr/bin/env bash
set -eo pipefail

TAG=$1
BRANCH=$2

cd "$SOURCE_ROOT"
if [[ ! -f RELEASED_VERSIONS ]]; then
    echo "RELEASED_VERSIONS file does not exist!"
    exit 1
fi

get_driver_relative_path() {
    if git -C "${LEGACY_DIR}" config --list -f .gitmodules --name-only | grep -q "falcosecurity-libs"; then
        echo "falcosecurity-libs"
    else
        echo "sysdig/src"
    fi
}

get_module_version() {
    if [[ -f "${LEGACY_DIR}/kernel-modules/MODULE_VERSION" ]]; then
        cat "${LEGACY_DIR}/kernel-modules/MODULE_VERSION"
    else
        echo ""
    fi
}

# get fingerprint from github
GH_KEY="$(ssh-keyscan github.com 2> /dev/null | grep ssh-rsa | head -n1)"
GH_FINGERPRINT="$(echo "${GH_KEY}" | ssh-keygen -lf - | cut -d" " -f2)"
# Verify from: https://help.github.com/en/articles/githubs-ssh-key-fingerprints
GH_FINGERPRINT_VERIFY="SHA256:nThbg6kXUpJWGl7E1IGOCspRomTxdCARLviKw6E5SY8"
if [[ "${GH_FINGERPRINT}" != "${GH_FINGERPRINT_VERIFY}" ]]; then
    echo >&2 "Unexpected SSH key fingerprint for github.com : ${GH_FINGERPRINT} != ${GH_FINGERPRINT_VERIFY}"
    exit 1
fi
mkdir -p ~/.ssh
echo "${GH_KEY}" > ~/.ssh/known_hosts

LEGACY_DIR="/tmp/old-sysdig"
git clone git@github.com:stackrox/collector "${LEGACY_DIR}"

# Use 'git@github.com' so the RSA key is properly used for cloning.
git config --global --add url.git@github.com:.insteadof https://github.com/

DRIVER_REL_DIR="$(get_driver_relative_path)"

mkdir -p "${WORKSPACE_ROOT}/ko-build"

# These directories contain a bi-directional mapping from collector to module version.
# The file structure allows using shell globbing without having to resort to `read` loops
# etc.
mkdir -p "${WORKSPACE_ROOT}/ko-build/"{released-collectors,released-modules}

while IFS='' read -r line || [[ -n "$line" ]]; do
    [[ -n "$line" ]] || continue

    collector_ref="$line"
    echo "Preparing module source archive for collector version ${collector_ref}"

    git -C "${LEGACY_DIR}" submodule deinit "${DRIVER_REL_DIR}"
    git -C "${LEGACY_DIR}" checkout "${collector_ref}"
    git -C "${LEGACY_DIR}" checkout -- .
    git -C "${LEGACY_DIR}" clean -xdf

    DRIVER_REL_DIR="$(get_driver_relative_path)"

    git -C "${LEGACY_DIR}" submodule update --init "${DRIVER_REL_DIR}"

    mod_ver_file="${WORKSPACE_ROOT}/ko-build/released-collectors/${collector_ref}"
    FALCO_DIR="${LEGACY_DIR}/${DRIVER_REL_DIR}" \
        SCRATCH_DIR="${HOME}/scratch" \
        OUTPUT_DIR="${HOME}/kobuild-tmp/versions-src" \
        MODULE_VERSION="$(get_module_version)" \
        ./kernel-modules/build/prepare-src | tail -n 1 \
        > "${mod_ver_file}"

    # If not building legacy probe version {module_version}, remove source 'kobuild-tmp/versions-src/{module_version}.tgz'
    # and do not add the collector version to 'ko-build/released-modules/{module_version}'.
    if [[ -z "$TAG" && "$BRANCH" != "master" && ! -f "${WORKSPACE_ROOT}/pr-metadata/labels/build-legacy-probes" ]]; then
        version="$(< "${mod_ver_file}")"
        [[ "$version" != "$MODULE_VERSION" ]] || continue
        echo "Not building probes for legacy version ${version}"
        rm "${HOME}/kobuild-tmp/versions-src/${version}.tgz"
    else
        echo "${collector_ref}" >> "${WORKSPACE_ROOT}/ko-build/released-modules/$(< "${mod_ver_file}")"
    fi

done < <(grep -v '^#' < RELEASED_VERSIONS | awk -F'#' '{print $1}' | awk 'NF==2 {print $1}' | sort | uniq)

rm -rf "${LEGACY_DIR}"

shopt -s nullglob
for i in "${WORKSPACE_ROOT}/ko-build/released-modules"/*; do
    version="$(basename "$i" .tgz)"
    [[ "$version" != "$MODULE_VERSION" ]] || continue
    echo "Building modules for legacy module version $version"
done
