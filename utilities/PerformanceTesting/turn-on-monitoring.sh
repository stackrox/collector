#!/usr/bin/env bash
set -eo pipefail

artifacts_dir=$1

export KUBECONFIG=$artifacts_dir/kubeconfig

# TODO put in a check for if monitoring is already on. Probably do that in another pr
(oc label namespace/stackrox openshift.io/cluster-monitoring="true" && sleep 120) || true
