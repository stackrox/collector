#!/usr/bin/env bash

set -e
GDOCKER_USER="$1"
shift
GDOCKER_PASS="$1"
shift
GSOURCE_ROOT="$1"
shift

source "$GSOURCE_ROOT/.circleci/ma.sh"
source "$GSOURCE_ROOT/.circleci/envbuilder.sh"

runCircleGCPUbuntuTestViaSSH

echo "A008"
exit 0
