#!/usr/bin/env bash
set -eo pipefail

shopt -s nullglob

NODE_INDEX=$1

shard_output_dir="${WORKSPACE_ROOT}/ko-build/build-output/shard-${NODE_INDEX}"
mkdir "${shard_output_dir}"
for task_file in ~/kobuild-tmp/local-build-tasks.*; do
  [[ -s "$task_file" ]] || continue
  flavor="$(basename "${task_file}")"
  flavor="${flavor#local-build-tasks\.}"
  echo "Building kernel modules with flavor $flavor"

  docker run --rm -i \
    -v "${HOME}/kobuild-tmp/bundles:/bundles:ro" \
    -v "${WORKSPACE_ROOT}/ko-build/module-versions:/sources:ro" \
    -v "${shard_output_dir}:/output" \
    --tmpfs /scratch:exec \
    "build-kernel-modules-${flavor}" \
    build-kos <"$task_file"
done
sudo chown -R "$(id -u):$(id -g)" "$shard_output_dir"
find "${shard_output_dir}/FAILURES" -type d -empty -depth -exec rmdir {} \;
if [[ -d "${shard_output_dir}/FAILURES" ]]; then
  mv "${shard_output_dir}/FAILURES" "${shard_output_dir}/../FAILURES-${NODE_INDEX}"
fi
