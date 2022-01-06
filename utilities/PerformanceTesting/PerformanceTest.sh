#!/usr/bin/env bash
set -eoux pipefail

name=$1
artifacts_dir=${2:-/tmp/artifacts}
collector_image_registry=${3:-docker.io/stackrox}
collector_image_tag=${4:-3.5.x-76-gaca574d047}

DIR="$(cd "$(dirname "$0")" && pwd)"

"$DIR"/CreateInfra.sh "$name" openshift-4 7h
"$DIR"/WaitForCluster.sh "$name"
"$DIR"/GetArtifactsDir.sh "$name" "$artifacts_dir"
export KUBECONFIG="$artifacts_dir"/kubeconfig
"$DIR"/StartCentralAndScanner.sh "$artifacts_dir"
"$DIR"/WaitForPods.sh "$artifacts_dir"
echo "Sleeping an additonal 120 seconds"
sleep 120
"$DIR"/GrabBundle.sh "$artifacts_dir"
"$DIR"/SecureCluster.sh "$artifacts_dir" "$collector_image_registry" "$collector_image_tag"
"$DIR"/TurnOnMonitoring.sh "$artifacts_dir"
