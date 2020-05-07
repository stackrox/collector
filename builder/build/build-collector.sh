#!/usr/bin/env bash
set -e

set -ux
ldconfig -v
cd /build-output
cmake -DCMAKE_BUILD_TYPE=Release /src
make all
strip --strip-unneeded ./collector
