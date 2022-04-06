#!/usr/bin/env bash
set -eo pipefail

NODE_TOTAL=$1
NODE_INDEX=$2

echo "Nodes total: $NODE_TOTAL"
echo "Node index:  $NODE_INDEX"

cd "${WORKSPACE_ROOT}/ko-build"
num_tasks=$(wc -l < build-tasks)
shard_size=$(((num_tasks - 1) / NODE_TOTAL + 1))

echo "Total number of tasks: ${num_tasks}"
echo "Tasks per shard: ${shard_size}"

mkdir -p ~/kobuild-tmp
cd ~/kobuild-tmp
split -d -l "$shard_size" "${WORKSPACE_ROOT}/ko-build/build-tasks" task-shard-

this_shard_file=~/kobuild-tmp/task-shard-"$(printf '%02d' "$NODE_INDEX")"

if [[ ! -s "$this_shard_file" ]]; then
    echo "Nothing to be done for this shard."
    circleci step halt
    exit 0
fi

mv "$this_shard_file" ~/kobuild-tmp/local-build-tasks

num_build_tasks="$(wc -l ~/kobuild-tmp/local-build-tasks | awk '{print $1}')"
echo "Building kernel modules for the following kernel/module version/probe type combinations:"
sed -e 's@^@  @' < ~/kobuild-tmp/local-build-tasks
echo "Total: ${num_build_tasks} kernel/module version/probe type combinations."

awk '{print $1}' < ~/kobuild-tmp/local-build-tasks | sort | uniq > ~/kobuild-tmp/all-kernel-versions
awk '{print $2}' < ~/kobuild-tmp/local-build-tasks | sort | uniq > ~/kobuild-tmp/all-module-versions

cd "${WORKSPACE_ROOT}/ko-build/build-output"
xargs < ~/kobuild-tmp/all-module-versions mkdir -p
