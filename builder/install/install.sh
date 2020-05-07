#!/usr/bin/env bash

set -e

# Versions installed from source via scripts in ./install/*sh
export B64_VERSION=1.2.1
export CARES_VERSION=1.16.0
export CMAKE_VERSION=3.5.1
export GOOGLETEST_REVISION=release-1.10.0
export GRPC_REVISION=v1.27.3
export JQ_VERSION=1.6
export JSONCPP_REVISION=0.10.7
export LUAJIT_VERSION=2.0.3
export NCURSES_VERSION=6_0_20150725
export PROMETHEUS_CPP_REVISION=v0.9.0
export PROTOBUF_VERSION=3.11.4
export TBB_VERSION=2018_U5

cd /install-tmp/ 
for f in [0-9][0-9]-*.sh; do 
  ./"$f"
done
cd && rm -rf /install-tmp
