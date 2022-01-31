#!/usr/bin/env bash

# shellcheck source=/dev/null
. <(declare -xp | grep '^declare -x COLLECTOR_ENV_' | sed -E 's/COLLECTOR_ENV_//')
  exec $COLLECTOR_PRE_ARGUMENTS collector "$@"
