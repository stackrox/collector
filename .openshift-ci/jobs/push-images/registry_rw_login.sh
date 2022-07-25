#!/usr/bin/env bash
set -eo pipefail

registry_rw_login() {
    if [[ "$#" -ne 1 ]]; then
        die "missing arg. usage: registry_rw_login <registry>"
    fi

    local registry="$1"

    case "$registry" in
        quay.io/rhacs-eng)
            if [[ -z "$QUAY_RHACS_ENG_RW_USERNAME" ]]; then
                echo "QUAY_RHACS_ENG_RW_USERNAME is not defined"
                exit 1
            fi
            if [[ -z "$QUAY_RHACS_ENG_RW_PASSWORD" ]]; then
                echo "QUAY_RHACS_ENG_RW_PASSWORD is not defined"
                exit 1
            fi
            docker login --username "$QUAY_RHACS_ENG_RW_USERNAME" --password-stdin quay.io <<< "$QUAY_RHACS_ENG_RW_PASSWORD"
            ;;
        quay.io/stackrox-io)
            if [[ -z "$QUAY_STACKROX_IO_RW_USERNAME" ]]; then
                echo "QUAY_STACKROX_IO_RW_USERNAME is not defined"
                exit 1
            fi
            if [[ -z "$QUAY_STACKROX_IO_RW_PASSWORD" ]]; then
                echo "QUAY_STACKROX_IO_RW_PASSWORD is not defined"
                exit 1
            fi
            docker login --username "$QUAY_STACKROX_IO_RW_USERNAME" --password-stdin quay.io <<< "$QUAY_STACKROX_IO_RW_PASSWORD"
            ;;
        *)
            echo "Unsupported registry login: $registry"
            ;;
    esac
}
