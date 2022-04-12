#!/usr/bin/env bash
set -eo pipefail

source_root=$1

git -C "${source_root}" submodule update --init
