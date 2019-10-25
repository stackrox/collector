#!/usr/bin/env bash

set -e
if [ -f /etc/redhat-release ]; then
  source "/opt/rh/devtoolset-6/enable"
fi

cd /install-tmp/ 
for f in [0-9][0-9]-*.sh; do 
  ./"$f"
  done
cd && rm -rf /install-tmp
