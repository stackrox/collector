#!/usr/bin/env bash

CI_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"
# shellcheck source=SCRIPTDIR=../../scripts/lib.sh
source "${CI_ROOT}/scripts/lib.sh"

export JOB_ID="${PROW_JOB_ID:0:8}"

export GCP_SSH_KEY_FILE="$HOME/.ssh/GCP_SSH_KEY"
export GCLOUD_INSTANCE="collector-osci-${IMAGE_FAMILY}-${JOB_ID}-update-support-packages"
export GCLOUD_OPTIONS="--ssh-key-file=${GCP_SSH_KEY_FILE}"
export REMOTE_HOST_TYPE=gcloud
export VM_CONFIG="${VM_TYPE}.${IMAGE_FAMILY}"
export COLLECTOR_REPO="quay.io/rhacs-eng/collector"

# TODO: make change ci user on GCP vms
export GCLOUD_USER="circleci"
if [[ "$VM_TYPE" == "flatcar" || "$VM_TYPE" =~ "coreos" ]]; then
    GCLOUD_USER="core"
fi

export WORKDIR=/go/src/github.com/stackrox/collector
export SOURCE_ROOT="./collector"
export SUPPORT_PKG_SRC_ROOT="$SOURCE_ROOT/kernel-modules/support-packages"

import_creds

"${CI_ROOT}/scripts/gcloud-init.sh"
