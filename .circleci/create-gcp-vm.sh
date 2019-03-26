#!/usr/bin/env bash

set -e
GDOCKER_USER="$1"
shift
GDOCKER_PASS="$1"
shift
GSOURCE_ROOT="$1"
shift

source "$GSOURCE_ROOT/.circleci/ma.sh"

CIRCLE_BUILD_VM_NAME="collector-nb-${CIRCLE_BUILD_NUM}"

createGCPVMUbuntu "$CIRCLE_BUILD_VM_NAME" "$GSOURCE_ROOT"
installVariousAptDepsViaGCPSSH "$CIRCLE_BUILD_VM_NAME"
loginDockerViaGCPSSH "$CIRCLE_BUILD_VM_NAME" "$GDOCKER_USER" "$GDOCKER_PASS"
extractSourceTarballViaGCPSSH "$CIRCLE_BUILD_VM_NAME"

echo "A008"
exit 0
