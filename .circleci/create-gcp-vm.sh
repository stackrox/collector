#!/usr/bin/env bash

set -e
main() {
  local GCP_VM_NAME="$1"
  shift
  local GDOCKER_USER="$1"
  shift
  local GDOCKER_PASS="$1"
  shift
  local GSOURCE_ROOT="$1"
  shift

  echo "A003"
  local BASHMODROOT="$GSOURCE_ROOT/.circleci" # will change later
  echo "A004"
  pushd "$BASHMODROOT"
  echo "A005"
  source moba.sh
  echo "A006"
  popd

  echo "A007"
  runGCPCosTestViaSSH "$GCP_VM_NAME" "$GDOCKER_USER" "$GDOCKER_PASS" "$GSOURCE_ROOT"
  echo "A008"
  return 0
}
