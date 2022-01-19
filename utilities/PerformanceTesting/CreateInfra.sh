#!/usr/bin/env bash
set -eou pipefail

name=$1
flavor=$2
lifespan=$3

does_cluster_exist() {
	nline="$({ infractl get "$name" 2> /dev/null || true; } | wc -l)"
	if (( "$nline" == 0 )); then
		cluster_exist="false"
	else
		cluster_exist="true"
	fi

	echo $cluster_exist
}


cluster_exist=$(does_cluster_exist)

if [[ $(does_cluster_exist) == "false" ]]; then
	echo "Creating an infra cluster with name $name, flavor $flavor"
	infractl create "$flavor" "$name" --description "Performance testing cluster"
	infractl lifespan "$name" "$lifespan"
else
	echo "Unable to create cluster"
fi
