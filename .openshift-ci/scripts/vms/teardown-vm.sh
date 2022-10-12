#!/usr/bin/env bash

set -eo pipefail

make -C ansible BUILD_TYPE=ci integration-tests-teardown
