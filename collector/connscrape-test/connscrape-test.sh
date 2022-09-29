#!/usr/bin/env bash
set -eo pipefail

nohup nc -l &
echo "Pid of nc command is $!"
/tmp/collector/cmake-build/collector/connscrape
