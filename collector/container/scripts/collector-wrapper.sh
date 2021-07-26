#!/usr/bin/env bash

if [[ $USE_VALGRIND = "true" ]] 
then
  valgrind_cmd='valgrind --leak-check=full'
else
  valgrind_cmd=''
fi

. <(declare -xp | grep '^declare -x COLLECTOR_ENV_' | sed -E 's/COLLECTOR_ENV_//')
  exec $valgrind_cmd collector "$@"
