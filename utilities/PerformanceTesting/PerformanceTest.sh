#!/usr/bin/env bash
set -eoux pipefail

cluster_name=$1
test_name=$2
collector_versions_file=${3:-collector_versions.txt}
teardown_script=${4:-$TEARDOWN_SCRIPT}
nrepeat=${5:-5}
artifacts_dir=${6:-/tmp/artifacts}


DIR="$(cd "$(dirname "$0")" && pwd)"

"$DIR"/CreateInfra.sh "$cluster_name" openshift-4 7h

test_dir="$DIR/TestResults/$test_name"
mkdir -p "$test_dir"

for ((n=0;n<nrepeat;n=n+1))
do
	while read -r line;
	do
		collector_image_registry="$(echo "$line" | awk '{print $1}')"
		collector_image_tag="$(echo "$line" | awk '{print $2}')"
		nick_name="$(echo "$line" | awk '{print $3}')"
		printf 'yes\n'  | $teardown_script
		"$DIR"/StartRox.sh "$cluster_name" "$artifacts_dir" "$collector_image_registry" "$collector_image_tag" 
		sleep 120
		"$DIR"/query.sh "$artifacts_dir" > "$test_dir/results_${nick_name}_${n}.txt"
	done < "$collector_versions_file"
done

printf 'yes\n'  | $teardown_script
