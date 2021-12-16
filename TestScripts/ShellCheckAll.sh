#!/usr/bin/env bash
set -eo pipefail

for file in $(find ../ -type f -name \*.sh)
do
  shellcheck $file
done

