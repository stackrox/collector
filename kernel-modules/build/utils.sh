#!/usr/bin/env bash

setup_environment() {
    driver_dir="$1"
    module_version="$2"

    SEMVER_RE='^([0-9]+)\.([0-9]+)\.([0-9]+)'

    API_VERSION="$(cat "${driver_dir}/API_VERSION")"
    [[ "$API_VERSION" =~ $SEMVER_RE ]] || exit 1
    export PPM_API_CURRENT_VERSION_MAJOR
    PPM_API_CURRENT_VERSION_MAJOR="${BASH_REMATCH[1]}"
    export PPM_API_CURRENT_VERSION_MINOR
    PPM_API_CURRENT_VERSION_MINOR="${BASH_REMATCH[2]}"
    export PPM_API_CURRENT_VERSION_PATCH
    PPM_API_CURRENT_VERSION_PATCH="${BASH_REMATCH[3]}"

    SCHEMA_VERSION="$(cat "${driver_dir}/SCHEMA_VERSION")"
    [[ "$SCHEMA_VERSION" =~ $SEMVER_RE ]] || exit 1
    export PPM_SCHEMA_CURRENT_VERSION_MAJOR
    PPM_SCHEMA_CURRENT_VERSION_MAJOR="${BASH_REMATCH[1]}"
    export PPM_SCHEMA_CURRENT_VERSION_MINOR
    PPM_SCHEMA_CURRENT_VERSION_MINOR="${BASH_REMATCH[2]}"
    export PPM_SCHEMA_CURRENT_VERSION_PATCH
    PPM_SCHEMA_CURRENT_VERSION_PATCH="${BASH_REMATCH[3]}"

    export DRIVER_NAME
    DRIVER_NAME="collector"
    export DRIVER_DEVICE_NAME
    DRIVER_DEVICE_NAME="${DRIVER_NAME}"
    export DRIVER_VERSION
    DRIVER_VERSION="${module_version}"
}

setup_driver_config() {
    falco_dir="$1"

    # Create the driver_config.h file
    envsubst < "${falco_dir}/driver/driver_config.h.in" > "${falco_dir}/driver/driver_config.h"

    if [[ -d "${falco_dir}/driver/bpf/configure" ]]; then
        for dir in "${falco_dir}"/driver/bpf/configure/*; do
            if [[ ! -d "${dir}" ]]; then
                continue
            fi

            CONFIGURE_MODULE=$(basename "$dir")
            CONFIGURE_ROOT="${falco_dir}/driver/bpf/${CONFIGURE_MODULE}"

            mkdir -p "${CONFIGURE_ROOT}"
            cp "${dir}/test.c" "${CONFIGURE_ROOT}"
            cp "${dir}/../Makefile" "${CONFIGURE_ROOT}"
            cp "${dir}/../build.sh" "${CONFIGURE_ROOT}"

            sed "s/@CONFIGURE_MODULE@/$CONFIGURE_MODULE/g" < "${dir}/../Makefile.inc.in" > "${CONFIGURE_ROOT}/Makefile.inc"
        done
    fi
}

compare_version() {
    version_re='^[0-9]+\.[0-9]+\.[0-9]+(:?-rc?[0-9]+)?$'

    if [[ ! "$1" =~ $version_re ]]; then
        echo >&2 "Error: Invalid version number format '$1'"
        exit 1
    fi

    if [[ ! "$2" =~ $version_re ]]; then
        echo >&2 "Error: Invalid version number format '$2'"
        exit 1
    fi

    IFS='.' read -ra first <<< "${1%-*}"
    IFS='.' read -ra second <<< "${2%-*}"

    for ((i = 0; i < ${#first[@]}; i++)); do
        if ((first[i] == second[i])); then
            continue
        fi

        if ((first[i] < second[i])); then
            echo "0"
            return 0
        fi
        echo "2"
        return 0
    done

    echo "1"
    return 0
}
