#!/usr/bin/env bash

. <(declare -xp | grep '^declare -x COLLECTOR_ENV_' | sed -E 's/COLLECTOR_ENV_//')
exec collector "$@"
