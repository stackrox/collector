#!/usr/bin/env bash
set -eo pipefail 

do_prometheus_query() {
  query_expression="$1"

  curl -sk -H "Authorization: Bearer $token" \
       --data-urlencode "query=${query_expression}" \
       https://"$url"/api/v1/query
}

get_value_from_query_result() {
	query_result=$1

	echo "$query_result" | jq -r '(.data.result | .[].value | .[1])'
}

do_prometheus_query_and_get_value() {
  query_expression="$1"

  query_result=$(do_prometheus_query "$query_expression")
  get_value_from_query_result "$query_result"
}
  
get_reports_for_collector_timers() {
  echo ""
  echo "#################"
  echo ""
  echo "Report for collector timers"

  total_max_time=0
  total_avg_time=0
  for timer in net_scrape_update net_scrape_read net_write_message net_create_message
  do
    avg_time_query='avg by (job) (rox_collector_timers{type="'${timer}'_times_us_avg"})'
    max_time_query='max by (job) (rox_collector_timers{type="'${timer}'_times_us_avg"})'

    avg_time=$(do_prometheus_query_and_get_value "$avg_time_query")
    max_time=$(do_prometheus_query_and_get_value "$max_time_query")

    total_avg_time=$(echo "$total_avg_time+$avg_time" | bc)
    total_max_time=$(echo "$total_max_time+$max_time" | bc)

    echo ""
    echo "Average time taken by $timer: $avg_time"
    echo "Maximum time taken by $timer in any pod: $max_time"
    echo ""
  done
  
  echo "Total time taken by collector timers in worst case (Sum of max times): $total_max_time"
  echo "Total average time taken by collector timers: $total_avg_time"
  echo ""
  echo "#################"
  echo ""
}

get_reports_for_collector_counters() {
  echo ""
  echo "#################"
  echo ""
  echo "Report for collector counters"

  for counter in net_conn_deltas net_conn_updates net_conn_inactive
  do
    avg_query='avg by (job) (rox_collector_counters{type="'${counter}'"})'
    max_query='max by (job) (rox_collector_counters{type="'${counter}'"})'

    avg=$(do_prometheus_query_and_get_value "$avg_query")
    max=$(do_prometheus_query_and_get_value "$max_query")

    echo ""
    echo "Average of $counter over pods: $avg"
    echo "Maximum of $counter over pods: $max"
    echo ""

  done
  echo ""
  echo "#################"
  echo ""
}

get_reports_for_sensor_network_flow() {
  echo ""
  echo "#################"
  echo ""
  echo "Report for sensor network flow"

  for flow_type in incoming outgoing
  do
     for protocol in L4_PROTOCOL_TCP L4_PROTOCOL_UDP
     do
       avg_query='avg by (job) (rox_sensor_network_flow_total_per_node{Protocol="'${protocol}'",Type="'${flow_type}'"})'
       max_query='max by (job) (rox_sensor_network_flow_total_per_node{Protocol="'${protocol}'",Type="'${flow_type}'"})'

       avg=$(do_prometheus_query_and_get_value "$avg_query")
       max=$(do_prometheus_query_and_get_value "$max_query")

       echo ""
       echo "Average number of $flow_type $protocol network flows over pods: $avg"
       echo "Maximum number of $flow_type $protocol network flows over pods: $max"
       echo ""
     done
  done

  for metric in rox_sensor_network_flow_host_connections_added rox_sensor_network_flow_host_connections_removed rox_sensor_network_flow_external_flows
  do
    query=$metric
    value=$(do_prometheus_query_and_get_value "$query")

    echo ""
    echo "$metric: $value"
    echo ""
  done

  echo ""
  echo "#################"
  echo ""

}

artifacts_dir=${1:-/tmp/artifacts}

export KUBECONFIG=$artifacts_dir/kubeconfig

password="$(cat "$artifacts_dir"/kubeadmin-password)"
printf '%s\n' "$password" | oc login -u kubeadmin

token="$(oc whoami -t)"
url="$(oc get routes -A | grep prometheus-k8s | awk '{print $3}' | head -1)"

get_reports_for_collector_timers
get_reports_for_collector_counters
get_reports_for_sensor_network_flow
