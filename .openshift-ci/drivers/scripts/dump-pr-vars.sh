#! /usr/bin/env bash

set -exo pipefail

if [[ -n "${JOB_SPEC:-}" ]]; then
    echo "export JOB_SPEC='${JOB_SPEC}'" >> /home/circleci/envvars.sh
fi
if [[ -n "${CLONEREFS_OPTIONS}" ]]; then
    echo "export CLONEREFS_OPTIONS='${CLONEREFS_OPTIONS}'" >> /home/circleci/envvars.sh
fi
if [[ -n "${PULL_BASE_REF:-}" ]]; then
    echo "export PULL_BASE_REF='${PULL_BASE_REF}'" >> /home/circleci/envvars.sh
fi
