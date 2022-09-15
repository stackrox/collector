#!/usr/bin/env bash
set -eo pipefail

mkdir /ci-data/
touch /ci-data/dump.sh

if [[ -n "${REPO_OWNER}" ]]; then
    echo "export REPO_OWNER='${REPO_OWNER}'" >> /ci-data/dump.sh
fi
if [[ -n "${REPO_NAME}" ]]; then
    echo "export REPO_NAME='${REPO_NAME}'" >> /ci-data/dump.sh
fi

if [[ -n "${PULL_NUMBER}" ]]; then
    echo "export PULL_NUMBER='${PULL_NUMBER}'" >> /ci-data/dump.sh
fi
if [[ -n "${PULL_BASE_REF}" ]]; then
    echo "export PULL_BASE_REF='${PULL_BASE_REF}'" >> /ci-data/dump.sh
fi

if [[ -n "${CLONEREFS_OPTIONS}" ]]; then
    echo "export CLONEREFS_OPTIONS='${CLONEREFS_OPTIONS}'" >> /ci-data/dump.sh
fi
if [[ -n "${JOB_SPEC}" ]]; then
    echo "export JOB_SPEC='${JOB_SPEC}'" >> /ci-data/dump.sh
fi
