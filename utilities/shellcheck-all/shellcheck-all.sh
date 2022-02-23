#!/usr/bin/env bash
set -eo pipefail

# Checks for potential errors and smells in shell scripts.
# This script runs shellcheck on all shell scripts in a specified directory or the base of a git project

die() {
    echo >&2 "$@"
    exit 1
}

if ! command -v shellcheck > /dev/null; then
    echo "shellcheck not installed. Please install shellcheck to run this script."
    exit 0
fi

if [[ -z "$1" ]]; then
    if command -v git > /dev/null; then
        BASE_DIR="$(git rev-parse --show-toplevel)" || die "Error: Not in a git repo. Specify a directory to scan or execute in a git repo."
    else
        die "Error: No directory to scan specified and git not installed."
    fi
else
    BASE_DIR=$1
fi

# shellcheck disable=SC2046
shellcheck $(find "$BASE_DIR" -type f -name \*.sh | grep -Ev '(/third_party/|/falcosecurity-libs/|/sysdig/)')
