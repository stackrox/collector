#!/usr/bin/env bash
set -eo pipefail

# shellcheck source=SCRIPTDIR/../create-vm.sh
source "$SOURCE_ROOT/.circleci/create-vm.sh"
main "$GCLOUD_INSTANCE" "$VM_TYPE" "$IMAGE_FAMILY" "$IMAGE_NAME" "$GCP_SSH_KEY_FILE" "$DOCKER_IO_PULL_USERNAME" "$DOCKER_IO_PULL_PASSWORD"
