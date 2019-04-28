#!/usr/bin/env bash

set -e
main() {
  local GCP_VM_NAME="$1"
  shift
  local GCP_VM_TYPE="$1"
  shift
  local GDOCKER_USER="$1"
  shift
  local GDOCKER_PASS="$1"
  shift
  local GSOURCE_ROOT="$1"
  shift

  local BASHMODROOT="$GSOURCE_ROOT/.circleci" # will change later
  pushd "$BASHMODROOT"
  source moba.sh
  popd

  setupGCPVM "$GCP_VM_NAME" "$GCP_VM_TYPE" "$GDOCKER_USER" "$GDOCKER_PASS"

  return 0
}
