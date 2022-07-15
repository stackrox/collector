#!/usr/bin/env bash
set -eo pipefail

registry_rw_login() {
    if [[ "$#" -ne 1 ]]; then
        die "missing arg. usage: registry_rw_login <registry>"
    fi

    local registry="$1"

    echo "registry= $registry"

    case "$registry" in
        docker.io/stackrox)
            echo "Trying to log into docker.io/stackrox"
            docker login -u "$DOCKER_IO_PUSH_USERNAME" -p "$DOCKER_IO_PUSH_PASSWORD" docker.io
            ;;
        quay.io/rhacs-eng)
            echo "Trying to log into quay.io/rhacs-eng"
            docker login -u "$QUAY_RHACS_ENG_RW_USERNAME" -p "$QUAY_RHACS_ENG_RW_PASSWORD" quay.io
            ;;
        quay.io/stackrox-io)
            echo "Trying to log into quay.io/stackrox-io"
            docker login -u "$QUAY_STACKROX_IO_RW_USERNAME" -p "$QUAY_STACKROX_IO_RW_PASSWORD" quay.io
            ;;
        *)
            echo "Unsupported registry login: $registry"
            ;;
    esac
}

shopt -s nullglob
for cred in /tmp/secret/**/[A-Z]*; do
    export "$(basename "$cred")"="$(cat "$cred")"
done

oc registry login

registry_rw_login quay.io/rhacs-eng

cd /go/src/github.com/stackrox/collector
COLLECTOR_VERSION="$(make tag)"

oc image mirror "$COLLECTOR_FULL" quay.io/rhacs-eng/collector:"$COLLECTOR_VERSION"