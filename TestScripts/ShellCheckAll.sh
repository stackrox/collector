#!/usr/bin/env bash
set -eo pipefail

#shellcheck checks for potential errors and smells in shell scripts.
#This script runs shellcheck on all shell scripts in Collector

if ! command -v shellcheck > /dev/null; then
  "shellcheck not installed. Please install shellcheck to run this script."
  exit 0
fi

SCRIPT_DIR=$(dirname "$0")
for file in $(find $SCRIPT_DIR/.. -type f -name \*.sh)
do
  shellcheck $file
done

