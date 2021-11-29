#!/usr/bin/env bash
set -eo pipefail

cat >~/workspace/shared-env \<<-"EOF"
  export GOPATH="${WORKSPACE_ROOT}/go"
  export SOURCE_ROOT="${GOPATH}/src/github.com/stackrox/collector"
  export COLLECTOR_SOURCE_ROOT="${SOURCE_ROOT}/collector"
  export PATH="${PATH}:${GOPATH}/bin:${WORKSPACE_ROOT}/bin"
  export MAX_LAYER_MB=300
EOF
