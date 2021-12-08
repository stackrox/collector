#!/usr/bin/env bash
set -eou pipefail

artifacts_dir=$1

echo "Grabing bundle"

export KUBECONFIG=$artifacts_dir/kubeconfig
central_password=`cat $artifacts_dir/kubeadmin-password`

rm perf-bundle.yml || true

roxctl -e https://`oc -n stackrox get routes central -o json | jq -r '.spec.host'`:443 \
   -p $central_password central init-bundles generate perf-test \
   --output perf-bundle.yml
