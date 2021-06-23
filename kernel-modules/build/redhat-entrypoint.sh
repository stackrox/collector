#!/usr/bin/env bash

exec scl enable devtoolset-8 llvm-toolset-7.0 -- "$@"
