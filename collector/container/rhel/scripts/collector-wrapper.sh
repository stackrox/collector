#!/usr/bin/env bash

COLLECTOR_PRE_ARGUMENTS=`echo $COLLECTOR_PRE_ARGUMENTS | tr -d "'"`
. <(declare -xp | grep '^declare -x COLLECTOR_ENV_' | sed -E 's/COLLECTOR_ENV_//')
  exec $COLLECTOR_PRE_ARGUMENTS collector "$@"
