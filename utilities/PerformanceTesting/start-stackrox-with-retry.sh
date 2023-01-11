#!/usr/bin/env bash
set -eou pipefail

teardown_script=$1
cluster_name=$2
artifacts_dir=${3:-/tmp/artifacts}
collector_image_registry=${4:-quay.io/rhacs-eng}
collector_image_tag=${5:-3.7.3}
bundle=${6:-perf-bundle-"${cluster_name}".yml}

DIR="$(cd "$(dirname "$0")" && pwd)"

while true; do
    exit_code=0
    "$DIR"/start-stack-rox.sh "$cluster_name" "$artifacts_dir" "$collector_image_registry" "$collector_image_tag" "$bundle" || exit_code=$?
    if [[ "$exit_code" -eq 0 ]]; then
        break
    else
	echo "Failed to deploy. Retrying"
        printf 'yes\n'  | $teardown_script
    fi
done
