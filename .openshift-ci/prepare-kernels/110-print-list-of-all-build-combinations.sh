#!/usr/bin/env bash
set -eo pipefail

cd "${WORKSPACE_ROOT}/ko-build"

num_build_tasks="$(wc -l build-tasks | awk '{print $1}')"
echo "Building kernel modules for the following kernel/module version/probe type combinations:"
sed -e 's@^@  @' < build-tasks
echo "Total: ${num_build_tasks} kernel/module version/probe type combinations."
