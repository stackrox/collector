#!/usr/bin/env bash

set -eo pipefail

make -C integration-tests/ansible BUILD_TYPE=ci teardown
