#!/usr/bin/env bash
set -eo pipefail

if ! command -v hub-comment; then
    wget --quiet https://github.com/joshdk/hub-comment/releases/download/0.1.0-rc6/hub-comment_linux_amd64
    hub_comment_bin=./hub-comment_linux_amd64
    sudo chmod 755 $hub_comment_bin
else
    hub_comment_bin=hub-comment
fi

perf_table=$(cat "$WORKSPACE_ROOT/benchmark.md")

export COLLECTOR_TAG="${COLLECTOR_TAG}"
export COLLECTOR_BUILDER_TAG="${COLLECTOR_BUILDER_TAG}"
export PERF_TABLE="$perf_table"

$hub_comment_bin -template-file "${CI_ROOT}/performance-comment-template.tpl"
