#! /usr/bin/env bash
set -eo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
errors=0

if ! make -C "${ROOT_DIR}/collector" check; then
    ((errors = errors + 1))
fi
if ! make -C "${ROOT_DIR}" shfmt-check; then
    ((errors = errors + 1))
fi
if ! make -C "${ROOT_DIR}" shellcheck-all; then
    ((errors = errors + 1))
fi

if ((errors)); then
    echo >&2 "${errors} linters have failed"
    exit 1
fi
