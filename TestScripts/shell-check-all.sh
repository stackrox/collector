#!/usr/bin/env bash
set -eo pipefail

#Checks for potential errors and smells in shell scripts.
#This script runs shellcheck on all shell scripts in Collector

if ! command -v shellcheck > /dev/null; then
  echo "shellcheck not installed. Please install shellcheck to run this script."
  exit 0
fi

SCRIPT_DIR=$(dirname "$0")

# shellcheck disable=SC2046
shellcheck $(find "$SCRIPT_DIR"/.. -type f -name \*.sh | grep -Ev '(/third_party/|/sysdig/)')
