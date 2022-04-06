#!/usr/bin/env bash
set -eo pipefail

#shellcheck disable=SC2002
cat /tmp/core_pattern | sudo tee /proc/sys/kernel/core_pattern
