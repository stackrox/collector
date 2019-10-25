#!/usr/bin/env bash

set -e

if cmake --version >/dev/null 2>&1 ; then
 echo >&2 "Not building cmake from source, already installed"
 exit 0
fi

wget "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz"
tar -zxf "cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz" --strip 1 -C "/usr/local/"
