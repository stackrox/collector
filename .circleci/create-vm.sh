#!/usr/bin/env bash

set -e
main() {
  local GCP_VM_NAME="$1"
  shift
  local GCP_VM_TYPE="$1"
  shift
  local GCP_SSH_KEY_FILE="$1"
  shift
  local GDOCKER_USER="$1"
  shift
  local GDOCKER_PASS="$1"
  shift

  DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

  pushd "$DIR"
  source "ma.sh"
  source "envbuilder.sh"
  popd

  setupGCPVM "$GCP_VM_NAME" "$GCP_VM_TYPE" "$GCP_SSH_KEY_FILE" "$GDOCKER_USER" "$GDOCKER_PASS"
}
