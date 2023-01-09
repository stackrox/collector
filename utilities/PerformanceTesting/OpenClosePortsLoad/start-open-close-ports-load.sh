#!/usr/bin/env bash
set -eoux pipefail

artifacts_dir=$1
num_ports=$2
num_per_second=$3

DIR="$(cd "$(dirname "$0")" && pwd)"

start_deployment() {
    echo "Starting deployment"
    kubectl delete secret myregistrykey || true
    kubectl delete deployment open-close-ports-load || true
    kubectl create -f "$DIR"/docker_registry_secret.yml
    kubectl create -f "$DIR"/deployment.yml
}

wait_for_pod() {
    echo "Waiting for pod"
    while true; do
        nterminating="$({ kubectl get pod | grep Terminating || true; } | wc -l)"
        if [[ "$nterminating" == 0 ]]; then
	    break
        fi
    done

    while true; do
	kubectl get pod
        ready_containers="$(kubectl get pod -o jsonpath='{.items[*].status.containerStatuses[?(@.ready == true)]}')"
        not_ready_containers="$(kubectl get pod -o jsonpath='{.items[*].status.containerStatuses[?(@.ready == false)]}')"
        if [[ -n "$ready_containers" && -z "$not_ready_containers" ]]; then
            echo "All pods are running"
            break
        fi
        sleep 1
    done
}

get_pod() {
    pod="$(kubectl get pod | grep open-close-ports-load | awk '{print $1}')"
}

start_load() {
    echo "Starting load"
    kubectl exec "$pod" -- python3 /open-close-ports-load.py "$num_ports" "$num_per_second" &
}

get_num_active_ports() {
    pod="$(kubectl get pod | grep open-close-ports-load | awk '{print $1}')"
    num_active_ports="$(kubectl exec "$pod" -- lsof -i -P -n | grep LISTEN | wc -l)"
    echo "$num_active_ports"
}

wait_for_load_to_equilibrate() {
    echo "Waiting for load to equilibrate"
    half_ports=$((num_ports / 2))
    num_active_ports="$(get_num_active_ports)"
    while [[ "$num_active_ports" -lt "$half_ports" ]]; do
        sleep 5
        num_active_ports="$(get_num_active_ports)"
    done
}

export KUBECONFIG="$artifacts_dir"/kubeconfig

start_deployment
wait_for_pod
get_pod
start_load
#wait_for_load_to_equilibrate
echo "Open close ports load started"
