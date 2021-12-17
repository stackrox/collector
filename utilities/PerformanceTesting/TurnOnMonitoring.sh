#!/usr/bin/env bash
set -eo pipefail

artifacts_dir=$1

export KUBECONFIG=$artifacts_dir/kubeconfig

oc label namespace/stackrox openshift.io/cluster-monitoring="true"
