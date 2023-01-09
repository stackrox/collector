#!/usr/bin/env bash
set -eou pipefail

artifacts_dir=$1
bundle=$2

echo "Grabing bundle"

export KUBECONFIG="$artifacts_dir/kubeconfig"
central_password="$(cat "$artifacts_dir"/kubeadmin-password)"

rm -f "$bundle"

url="$(oc -n stackrox get routes central -o json | jq -r '.spec.host')"
roxctl -e https://"$url":443 \
    -p "$central_password" central init-bundles generate perf-test \
    --output "$bundle"
