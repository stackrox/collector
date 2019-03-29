#!/usr/bin/env bash

set -e
main() {
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

  runCircleGCPUbuntuTestViaSSH "$GDOCKER_USER" "$GDOCKER_PASS" "$GDOCKER_ROOT"

  echo "A008"
  return 0
}
