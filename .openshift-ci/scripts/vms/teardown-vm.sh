#!/usr/bin/env bash

set -eo pipefail

make -C integration-tests/ansible teardown
