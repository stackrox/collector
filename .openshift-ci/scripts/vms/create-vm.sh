#!/usr/bin/env bash

set -eo pipefail

CI_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"

# shellcheck source=SCRIPTDIR/../lib.sh
source "${CI_ROOT}/scripts/lib.sh"

# shellcheck source=SCRIPTDIR/provision.sh
source "${CI_ROOT}/scripts/vms/provision.sh"

main() {
    local GCP_SSH_KEY_FILE="$1"
    shift

    mkdir -p "$(dirname "${GCP_SSH_KEY_FILE}")"
    chmod 0700 "$(dirname "${GCP_SSH_KEY_FILE}")"

    copy_secret_to_file GCP_SSH_KEY "${GCP_SSH_KEY_FILE}" 0600
    copy_secret_to_file GCP_SSH_KEY_PUB "${GCP_SSH_KEY_FILE}.pub" 0600

    VM_TYPE=rhel make -C integration-tests/ansible setup
}

main "${GCP_SSH_KEY_FILE}"
