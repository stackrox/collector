#!/usr/bin/env bash
set -eou pipefail

artifacts_dir=$1
test_name=$2
kubenetbench_bin=${3:-$KUBENETBENCH_BIN}

DIR="$(cd "$(dirname "$0")" && pwd)"

export KUBECONFIG=$artifacts_dir/kubeconfig

cd "$DIR/TestResults"
kubenetbench_test_name="kubenetbench_$test_name"

"$kubenetbench_bin" -s "$kubenetbench_test_name" init &
sleep 120
./"$kubenetbench_test_name"/knb pod2pod &
sleep 120

cd -
