#!/usr/bin/env bash
set -eoux pipefail

name=$1
artifacts_dir=${2:-/tmp/artifacts}

DIR="$(cd "$(dirname "$0")" && pwd)"

"$DIR"/CreateInfra.sh "$name" openshift-4 7h
"$DIR"/WaitForCluster.sh "$name"
"$DIR"/GetArtifactsDir.sh "$name" "$artifacts_dir"
export KUBECONFIG="$artifacts_dir"/kubeconfig
"$DIR"/StartCentralAndSensor.sh "$artifacts_dir"
"$DIR"/WaitForPods.sh "$artifacts_dir"
echo "Sleeping an additonal 120 seconds"
sleep 120
"$DIR"/GrabBundle.sh "$artifacts_dir"
"$DIR"/SecureCluster.sh "$artifacts_dir"
"$DIR"/TurnOnMonitoring.sh "$artifacts_dir"
