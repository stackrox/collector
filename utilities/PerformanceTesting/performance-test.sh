#!/usr/bin/env bash
set -eoux pipefail

json_config_file=$1

DIR="$(cd "$(dirname "$0")" && pwd)"

cluster_name="$(cat "$json_config_file" | jq .cluster_name | tr -d \")"
if [ "$cluster_name" == "null" ]; then
    echo "cluster_name must be defined"
fi

test_dir="$(cat "$json_config_file" | jq .test_dir | tr -d \")"
if [ "$test_dir" == "null" ]; then
    echo "test_dir must be defined"
    exit 1
fi

nrepeat="$(cat "$json_config_file" | jq .nrepeat | tr -d \")"
if [ "$nrepeat" == "null" ]; then
    nrepeat=5
    echo "nrepeat set to $nrepeat"
fi

nodes="$(cat "$json_config_file" | jq .nodes | tr -d \")"
if [ "$nodes" == "null" ]; then
    nodes=3
    echo "nodes set to $nodes"
fi

artifacts_dir="$(cat "$json_config_file" | jq .artifacts_dir | tr -d \")"
if [ "$artifacts_dir" == "null" ]; then
    artifacts_dir="/tmp/artifacts-${cluster_name}"
fi

load="$(cat "$json_config_file" | jq .load)"
if [ "$load" == "null" ]; then
    echo "load must be defined"
    exit 1
fi
load_duration="$(echo $load | jq .load_duration| tr -d \")"
kubenetbench_load="$(echo $load | jq .kubenetbench_load)"
if [ "$kubenetbench_load" != null ]; then
    num_streams="$(echo "$kubenetbench_load" | jq .num_streams | tr -d \")"
    load_test_name="$(echo "$kubenetbench_load" | jq .load_test_name | tr -d \")"

    if [ "$num_streams" == "null" ]; then
        echo "If kubenetbench_load is defined num_streams must be defined"
	exit 1
    fi
    if [ "$load_test_name" == "null" ]; then
        echo "If kubenetbench_load is defined load_test_name must be defined"
	exit 1
    fi
fi
open_close_ports_load="$(echo $load | jq .open_close_ports_load)"
if [ "$open_close_ports_load" != null ]; then
    num_ports="$(echo "$open_close_ports_load" | jq .num_ports | tr -d \")"
    num_per_second="$(echo "$open_close_ports_load" | jq .num_per_second | tr -d \")"
    num_pods="$(echo "$open_close_ports_load" | jq .num_pods | tr -d \")"

    
    if [ "$num_ports" == "null" ]; then
        echo "If open_close_ports_load is defined num_ports must be defined"
	exit 1
    fi
    if [ "$num_per_second" == "null" ]; then
        echo "If open_close_ports_load is defined num_per_second must be defined"
	exit 1
    fi
    if [ "$num_pods" == "null" ]; then
        echo "If open_close_ports_load is defined num_pods must be defined"
	exit 1
    fi
fi

teardown_script="$(cat "$json_config_file" | jq .teardown_script | tr -d \")"
if [ "$teardown_script" == "null" ]; then
    if [ -z "$TEARDOWN_SCRIPT" ]; then
       echo "Teardown script must be defined"
    else
       teardown_script="$TEARDOWN_SCRIPT"
    fi
fi

sleep_after_start_rox="$(cat "$json_config_file" | jq .sleep_after_start_rox | tr -d \")"
if [ "$sleep_after_start_rox" == "null" ]; then
    echo "sleep_after_start_rox must be defined"
fi

query_window="$(cat "$json_config_file" | jq .query_window | tr -d \")"
if [ "$query_window" == "null" ]; then
    echo "query_window must be defined"
fi

versions="$(cat "$json_config_file" | jq .versions)"
if [ "$versions" == "null" ]; then
    echo "versions must be defined"
fi

nversion="$(cat "$json_config_file" | jq '.versions | length')"

"$DIR"/create-infra.sh "$cluster_name" "$nodes" openshift-4 48h
"$DIR"/wait-for-cluster.sh "$cluster_name"
"$DIR"/get-artifacts-dir.sh "$cluster_name" "$artifacts_dir"
export KUBECONFIG="$artifacts_dir"/kubeconfig
if [[ $kubenetbench_load != "null" && $num_streams -gt 0 ]]; then
    kubectl delete ds knb-monitor || true
    kubectl delete pod knb-cli || true
    kubectl delete pod knb-srv || true
    knb_base_dir="$(mktemp -d)"
    "$DIR/initialize-kubenetbench.sh" "$artifacts_dir" "$load_test_name" "$knb_base_dir"
fi

mkdir -p "$test_dir"

for ((n = 0; n < nrepeat; n = n + 1)); do
    for ((i = 0; i < nversion; i = i + 1)); do
	version="$(echo $versions | jq .["$i"])"
        echo "$version"
	collector_image_registry="$(echo $version | jq .collector_image_registry | tr -d \")"
	collector_image_tag="$(echo $version | jq .collector_image_tag | tr -d \")"
	nick_name="$(echo $version | jq .nick_name | tr -d \")"
	patch_script="$(echo $version | jq .patch_script | tr -d \")"
	env_var_file="$(echo $version | jq .env_var_file | tr -d \")"
        source "${env_var_file}"
        if [[ $open_close_ports_load != "null" && $num_ports -gt 0 ]]; then
            "$DIR"/OpenClosePortsLoad/start-open-close-ports-load.sh "$artifacts_dir" "$num_ports" "$num_per_second" "$num_pods"
        fi
        printf 'yes\n'  | $teardown_script
        "$DIR"/start-stackrox-with-retry.sh "$teardown_script" "$cluster_name" "$artifacts_dir" "$collector_image_registry" "$collector_image_tag"
	"$DIR"/wait-for-pods.sh "$artifacts_dir"
        "$patch_script" "$artifacts_dir"
	"$DIR"/wait-for-pods.sh "$artifacts_dir"
        sleep "$sleep_after_start_rox"
        if [[ $kubenetbench_load != "null" && $num_streams -gt 0 ]]; then
            "$DIR/generate-load.sh" "$artifacts_dir" "$load_test_name" "$num_streams" "$knb_base_dir" "$load_duration"
	else
	    sleep "$load_duration"
        fi
        kubectl get pod --namespace stackrox
        query_output="$test_dir/results_${nick_name}_${n}.json"
        "$DIR"/query.sh "$query_output" "$artifacts_dir" "$query_window"
        if [[ $open_close_ports_load != "null" && $num_ports -gt 0 ]]; then
            "$DIR"/OpenClosePortsLoad/stop-open-close-ports-load.sh "$artifacts_dir"
	fi
    done
done

printf 'yes\n'  | $teardown_script

if [[ $kubenetbench_load != "null" && $num_streams -gt 0 ]]; then
    "$DIR/teardown-kubenetbench.sh" "$artifacts_dir" "$knb_base_dir"
fi

for ((i = 0; i < nversion; i = i + 1)); do
    version="$(echo $versions | jq .["$i"])"
    nick_name="$(echo $version | jq .nick_name | tr -d \")"
    python3 "$DIR"/GetAverages.py "${test_dir}/results_${nick_name}_" "$nrepeat" "${test_dir}/Average_results_${nick_name}.json"
done
