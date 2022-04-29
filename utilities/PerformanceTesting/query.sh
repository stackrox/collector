#!/usr/bin/env bash
set -eo pipefail

output_file=${1:-}
artifacts_dir=${2:-/tmp/artifacts}
query_window=${3:-10m}

export KUBECONFIG=$artifacts_dir/kubeconfig

oc login -u kubeadmin < "$artifacts_dir/kubeadmin-password"

token="$(oc whoami -t)"
url="$(oc get routes -A | grep prometheus-k8s | awk '{print $3}' | head -1)"

url=https://"$url"/api/v1/query

python3 query.py "$query_window" "$token" "$url" "$output_file"
