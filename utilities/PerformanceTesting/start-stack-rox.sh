#!/usr/bin/env bash
set -eou pipefail

name=$1
artifacts_dir=${2:-/tmp/artifacts}
ARCH=$(arch)
if [ "$ARCH" = "x86_64" ]; then
    collector_image_registry=${3:-quay.io/rhacs-eng}
else
    collector_image_registry=${3:-quay.io/rhacs-eng/$ARCH}
fi
collector_image_tag=${4:-3.7.3}

DIR="$(cd "$(dirname "$0")" && pwd)"

"$DIR"/wait-for-cluster.sh "$name"
"$DIR"/get-artifacts-dir.sh "$name" "$artifacts_dir"
export KUBECONFIG="$artifacts_dir"/kubeconfig
"$DIR"/start-central-and-scanner.sh "$artifacts_dir"
"$DIR"/wait-for-pods.sh "$artifacts_dir"
"$DIR"/grab-bundle.sh "$artifacts_dir"
"$DIR"/start-secured-cluster.sh "$artifacts_dir" "$collector_image_registry" "$collector_image_tag"
"$DIR"/turn-on-monitoring.sh "$artifacts_dir"
