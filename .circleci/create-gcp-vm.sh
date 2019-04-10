#!/usr/bin/env bash

set -e
main() {
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
  runGCPCosTestViaSSH "$GDOCKER_USER" "$GDOCKER_PASS" "$GDOCKER_ROOT" "$GSOURCE_ROOT"
  echo "A008"
  return 0
}
