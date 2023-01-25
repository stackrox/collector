#!/usr/bin/env bash
set -eou pipefail

dir=$1

metrics=(
	.collector_timers.net_create_message.Average 
	.collector_timers.net_scrape_read.Average 
	.collector_timers.collector_timers_total.Average 
	.collector_counters.net_cep_deltas.Average 
	.cpu_mem_network_usage.cpu_usage.Average 
	.cpu_mem_network_usage.collector_cpu_usage.Average 
	.cpu_mem_network_usage.sensor_cpu_usage.Average 
	.cpu_mem_network_usage.central_cpu_usage.Average 
	.cpu_mem_network_usage.central_db_cpu_usage.Average 
	.cpu_mem_network_usage.scanner_cpu_usage.Average 
	.cpu_mem_network_usage.scanner_db_cpu_usage.Average 
	.cpu_mem_network_usage.mem_usage.Average 
	.cpu_mem_network_usage.collector_mem_usage.Average 
	.cpu_mem_network_usage.sensor_mem_usage.Average 
	.cpu_mem_network_usage.central_mem_usage.Average 
	.cpu_mem_network_usage.central_db_mem_usage.Average
	.cpu_mem_network_usage.scanner_mem_usage.Average 
	.cpu_mem_network_usage.scanner_db_mem_usage.Average
	.pod_restarts.collector_restarts.Average
	.pod_restarts.sensor_restarts.Average
	.pod_restarts.central_restarts.Average
	.pod_restarts.scanner_restarts.Average
	.central_metrics.postgres_op_duration_ProcessListeningOnPortStorage_Remove.value
	.central_metrics.postgres_op_duration_ProcessListeningOnPortStorage_RemoveMany.value
	.central_metrics.postgres_op_duration_ProcessListeningOnPortStorage_UpdateMany.value
	.central_metrics.datastore_function_duration_sum_AddProcessListeningOnPort.value
)

get_nick_names() {
    json_config_file="$dir/config.json"
    versions="$(jq .versions "$json_config_file")"
    nick_names=()
    versions="$(jq .versions "$json_config_file")"
    nversion="$(jq '.versions | length' "$json_config_file")"
    for ((i = 0; i < nversion; i = i + 1)); do
	version="$(echo $versions | jq .["$i"])"
        nick_name="$(echo $version | jq .nick_name | tr -d \")"
	nick_names+=($nick_name)
    done
    for nick_name in "${nick_names[@]}"; do
            echo "nick_name= $nick_name"
    done
}

plot_results() {
    local metric_csv_file=$1
    local metric_name=$2
    local units=$3

    metric_name="$(echo $metric_name | sed 's|_|\\_|g')"

    gnuplot_script="$dir/temp.gnu"
    output="$(echo $metric_csv_file | sed 's|.csv$|.png|')"

    echo "set term png" > "$gnuplot_script"
    echo "set title '$metric_name'" >> "$gnuplot_script"
    echo "set xlabel 'num ports'" >> "$gnuplot_script"
    echo "set ylabel '$units'" >> "$gnuplot_script"
    echo "set key bottom right" >> "$gnuplot_script"
    echo "set datafile separator ','" >> "$gnuplot_script"
    echo "set key autotitle columnhead" >> "$gnuplot_script"
    echo "set output '$output'" >> "$gnuplot_script"

    line="plot"
    len_nick_name=${#nick_names[@]}
    for ((i=0; i<${#nick_names[@]}; i=i+1)); do
	column=$((i + 2))
        line="$line '$metric_csv_file' using 1:$column with lines lw 3"
	if [[ $i -lt $((len_nick_name - 1)) ]]; then
	    line="$line,"
	fi
    done

    echo "$line" >> "$gnuplot_script"

    gnuplot "$gnuplot_script"
}

get_header() {
    header="num_ports"
    for nick_name in "${nick_names[@]}"; do
        header="$header,$nick_name"
    done
    header="$(echo "$header" | sed 's|_| |')"

    echo "$header"
}

get_nick_names
header="$(get_header)"
for metric in ${metrics[@]}; do
    top_level_metric="$(echo $metric | sed 's|.Average||' | sed 's|.value||')"
    metric_name="$(echo "$top_level_metric" | sed 's|.*\.||')"
    metric_csv_file="$dir/$metric_name".csv
    units="$(cat $dir/num_ports_100/Average_results_"${nick_names[0]}".json | jq $top_level_metric.units)"
    echo $metric.Average,$units
    echo "$header"
    echo "$header" > "$metric_csv_file"
    for ports in 100 200 400; do
    #for ports in 100 200 400 800 1600 3200 6400 12800; do
        line="$ports"
        for nick_name in ${nick_names[@]}; do
            line="$line,$(cat "$dir"/num_ports_${ports}/Average_results_"$nick_name".json | jq "$metric")"
        done
        echo "$line"
        echo "$line" >> "$metric_csv_file"
    done
    plot_results "$metric_csv_file" "$metric_name" "$units"
    echo
    echo
done
