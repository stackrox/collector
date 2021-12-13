#!/usr/bin/env bash
set -eo pipefail 

token=`oc whoami -t`
url=`oc get routes -A | grep prometheus-k8s | awk '{print $3}' | head -1`

query_expressions=(
	'rox_collector_timers{type="net_scrape_update_times_us_avg"}'
	'rox_collector_timers{type="net_scrape_read_times_us_avg"}'
	'rox_collector_timers{type="net_write_message_times_us_avg"}'
	'rox_collector_timers{type="net_create_message_times_us_avg"}'
	'rox_collector_counters{type="net_con_deltas"}'
	'rox_collector_counters{type="net_con_updates"}'
	'rox_collector_counters{type="net_con_inactive"}'
	)

for query_expression in ${query_expressions[@]}
do
  echo ""
  echo "###############################"
  echo $query_expression
  echo ""
  curl -k -H "Authorization: Bearer $token" \
       --data-urlencode "query=${query_expression}" \
       https://$url/api/v1/query
  echo ""
  echo "###############################"
  echo ""
done
