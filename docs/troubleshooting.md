# Collector Troubleshooting

This document describes common error conditions that Collector may encounter, and
provides steps that can be taken to diagnose the cause, fix potential issues and
return Collector to a working state.

If the troubleshooting steps below do not solve the issue, please contact ACS
support.

## First Steps

### Logs

The first thing to look at are the logs from any failing Collectors. The troubleshooting
guides below will offer examples of log messages you can expect to see for a number of
error conditions, so having access to your own logs is invaluable.

Depending on the environment, and accesses, you can get these in a number of ways.

#### Kubectl

Kubernetes is the easiest way to retrieve the logs, provided you have access to
do so.

```sh
$ kubectl get pods -n stackrox -l app=collector
collector-vclg5                      1/2     CrashLoopBackOff   2 (25s ago)   2m41s

$ kubectl logs -n stackrox collector-vclg5 collector

# sometimes it is also useful to view the previous logs of collector,
# if the current restart is running
$ kubectl logs -n stackrox collector-vclg5 collector --previous
```


#### OC

Since the `oc` tool maintains compatibility with kubectl, the required commands are
much the same.

```sh
$ oc get pods -n stackrox -l app=collector
collector-vclg5                      1/2     CrashLoopBackOff   2 (25s ago)   2m41s

$ oc logs -n stackrox collector-vclg5 collector
$ oc logs -n stackrox collector-vclg5 collector --previous
```

#### Diagnostic Bundle

An alternative method is to download a diagnostic bundle from the ACS UI, by
following the instructions [here](https://docs.openshift.com/acs/configuration/generate-diagnostic-bundle.html)

Within the downloaded bundle, the logs for all Collector pods will be available
for inspection.

### Log level

Collector features several log levels:

* TRACE: The most verbose one, contains e.g. relocations for the eBPF program
  and processed events. Not recommended turning on in production due to large
  performance overhead.
* DEBUG: Log information essential for debugging, reasonable to use without
  taking large performance overhead.
* INFO: The default log level.
* WARNING: Contains information about unexpected situations.
* ERROR: Reports processing failures, without stopping Collector.
* FATAL: Unrecoverable errors, duplicated to the termination log.

There are a few ways to configure log level in Collector, all are
case-insensitive:

* Via environment variable `ROX_COLLECTOR_LOG_LEVEL`.

* In the Collector configuration via the `logLevel` key. This option takes
  precedence over the environment variable.

* Via API call `/loglevel`. This will enforce the log level value, regardless
  of existing configuration.

```
$ curl -X POST -d "trace" collector:8080/loglevel
```

### Pod Status

Another way of getting a quick look at the reason for a crashing Collector is to
look at the last state of the Pod. Any failure messages are written to the last
status, and can be viewed using `kubectl` or `oc`:

```sh
# substitute your collector pod into this command
$ kubectl describe pod -n stackrox collector-vclg5
[...]
    Last State:     Terminated
      Reason:       Error
      Message:      No suitable kernel object downloaded
      Exit Code:    1
      Started:      Fri, 21 Oct 2022 11:50:56 +0100
      Finished:     Fri, 21 Oct 2022 11:51:25 +0100
[...]
```

We can instantly see that the Collector has failed to download a kernel driver.

## Collector Start Up

The vast majority of errors occur during Collector startup, where Collector
will configure itself, load the eBPF probe into the kernel and then start
collecting events.

If any part of the start-up procedure fails, a helpful diagnostic summary is written
to the logs, detailing which steps succeeded or failed.

For example, for a successful startup, where Collector has been able to connect
to Sensor and load the eBPF probe successfully into the kernel:

```
[INFO    2025/07/24 10:05:54] == Collector Startup Diagnostics: ==
[INFO    2025/07/24 10:05:54]  Connected to Sensor?       false
[INFO    2025/07/24 10:05:54]  Kernel driver candidates:
[INFO    2025/07/24 10:05:54]    core_bpf (available)
[INFO    2025/07/24 10:05:54]  Driver loaded into kernel: core_bpf
[INFO    2025/07/24 10:05:54] ====================================
```

## Common Error Conditions

### Unable to Connect to Sensor

The first thing that Collector attempts to do upon starting is connect to
Sensor. Sensor is used to download CIDR blocks for processing network events.
As such, it is fundamental to the rest of the start up.

```
Collector Version: 3.12.0
OS: Ubuntu 20.04.4 LTS
Kernel Version: 5.4.0-126-generic
Starting StackRox Collector...
[INFO    2022/10/13 12:20:43] Hostname: 'hostname'
[...]
[INFO    2022/10/13 12:20:43] Sensor configured at address: sensor.stackrox.svc:9998
[INFO    2022/10/13 12:20:43] Attempting to connect to Sensor
[INFO    2022/10/13 12:21:13]
[INFO    2022/10/13 12:21:13] == Collector Startup Diagnostics: ==
[INFO    2022/10/13 12:21:13]  Connected to Sensor?       false
[INFO    2022/10/13 12:21:13]  Kernel driver candidates:
[INFO    2022/10/13 12:21:13] ====================================
[INFO    2022/10/13 12:21:13]
[FATAL   2022/10/13 12:21:13] Unable to connect to Sensor at 'sensor.stackrox.svc:9998'.
```

Normally, failing to connect to Sensor either means that Sensor has not started
correctly or Collector has been misconfigured. Double check the Collector configuration
to ensure that the Sensor address is correct, and check that the Sensor pod is running
correctly.

### Failed to load the eBPF probe

In rare cases, there may be a problem with loading the eBPF probe. This is the
final step before Collector is fully up and running, and failures can result in a
variety of error messages or exceptions. The same diagnostic summary is reported
in the logs, and will indicate the failure of this step.

If an error of this kind is encountered, it is unlikely that it can be easily
fixed, so should be reported to ACS support or in a [GitHub issue](https://github.com/stackrox/collector/issues).

The following is a simple example of this occurring:

```
[...]
[INFO    2025/07/24 10:26:37] Trying to open the right engine!
[INFO    2025/07/24 10:26:41] libbpf: prog 'execve_x': -- BEGIN PROG LOAD LOG --
[...]
-- END PROG LOAD LOG --
[INFO    2025/07/24 10:26:41] libbpf: prog 'execve_x': failed to load: -7
[INFO    2025/07/24 10:26:41] libbpf: failed to load object 'bpf_probe'
[INFO    2025/07/24 10:26:41] libbpf: failed to load BPF skeleton 'bpf_probe': -7
[INFO    2025/07/24 10:26:41] libpman: failed to load BPF object (errno: 7 | message: Argument list too long)
```

## Troubleshooting using performance counters

The collector publishes some performance counters that can be used to investigate runtime issues.

The runtime values are exposed via the Prometheus endpoint `/metrics` and can be accessed on port 9090.

Timers guard some portions of the code to measure the amount of time that is spent running their content.
For each timer, 3 values are published. They are named after the name of the timer suffixed by:

- `_events`: the total number of occurences the monitored code was run.
- `_us_total`: accumulated time spent running the monitored code, in micro-seconds.
- `_us_avg`: the mean duration, computed from the two previous values.

### Network status notifier timers

```
Component: CollectorStats
Prometheus name: rox_collector_timers
Units: microseconds
```

| Name                                             | Description                                                                                                                          |
|--------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------|
| net_scrape_read                                  | Time spent iterating over /proc content to retrieve connections and endpoints for each process.                                      |
| net_scrape_update                                | Time spent updating the internal model with information read from /proc (set removed entries as inactive, update activity timestamp) |
| net_fetch_state                                  | Time spent to build a delta message content (connections + endpoints) to send to Sensor                                              |
| net_create_message                               | Time spent to serialize the delta message and store the resulting state for next computation.                                        |
| net_write_message                                | Time spent sending the raw message content.                                                                                          |
| process_info_wait                                | Time spent blocked waiting for process info to be resolved by system_inspector.                                                      |


### Network status notifier counters

```
Component: CollectorStats
Prometheus name: rox_collector_counters
Units: occurence
```

| Name                                             | Description                                                                                                                          |
|--------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------|
| net_conn_updates                                 | Each time a connection object is updated in the model (scrapes, and kernel events).                                                  |
| net_conn_deltas                                  | Number of connection events sent to Sensor.                                                                                          |
| net_conn_inactive                                | Accumulated number of connections destroyed (closed)                                                                                 |
| net_cep_updates                                  | Each time an endpoint object is updated in the model (scrapes only).                                                                 |
| net_cep_deltas                                   | Number of endpoint events sent to Sensor.                                                                                            |
| net_cep_inactive                                 | Accumulated number of endpoints destroyed (closed)                                                                                   |
| net_known_ip_networks                            | Number of known-networks defined.                                                                                                    |
| net_known_public_ips                             | Number of known public addresses defined.                                                                                            |
| process_lineage_counts                           | Every time the lineage info of a process is created (signal emitted) \[1\]                                                             |
| process_lineage_total                            | Total number of ancestors reported \[1\]                                                                                               |
| process_lineage_sqr_total                        | Sum of squared number of ancestors reported \[1\]                                                                                      |
| process_lineage_string_total                     | Accumulated size of the lineage process exec file paths \[1\]                                                                          |
| process_info_hit                                 | Accessing originator process info of an endpoint with data readily available.                                                        |
| process_info_miss                                | Accessing originator process info of an endpoint ends-up waiting for Falco to resolve data.                                          |
| rate_limit_flushing_counts                       | Number of overflows in the rate limiter used to send process signals.                                                                |

\[1\] the process lineage information contains the ancestors list of a process. This attribute is formatted as a list of
the process exec file paths.

### system_inspector counters

```
Component: system_inspector::Stats
Prometheus name: rox_collector_events
Units: occurence
```

| Name                                   | Description                                                                                         |
|----------------------------------------|-----------------------------------------------------------------------------------------------------|
| kernel                                 | number of received kernel events (by the probe)                                                     |
| drops                                  | number of dropped kernel events                                                                     |
| threadCacheDrops                       | number of dropped kernel events due to threadcache being full                                       |
| ringbufferDrops                        | number of dropped kernel events due to ringbuffer being full                                        |
| preemptions                            | Number of preemptions (?)                                                                           |
| userspace[syscall]                     | Number of this kind of event                                                                        |
| grpcSendFailures                       | (not used?)                                                                                         |
| threadCacheSize                        | Number of thread-info entries stored in the thread cache (sampled every 5s)                         |
| processSent                            | Process signal sent with success                                                                    |
| processSendFailures                    | Failure upon sending a process signal                                                               |
| processResolutionFailuresByEvt         | Count of invalid process signal events received, then ignored (invalid path or name, or not execve) |
| processResolutionFailuresByTinfo       | Count of invalid process found parsed during initial iteration (existing processes)                 |
| processRateLimitCount                  | Count of processes not sent because of the rate limiting.                                           |
| parse_micros[syscall]                  | Total time used to retrieve an event of this type from falco                                        |
| process_micros[syscall]                | Total time used to handle/send an event of this type (call the SignalHandler)                       |
| procfs_could_not_get_network_namespace | Count of the number of times that ProcfsScraper was unable to get the netwrok namespace             |
| procfs_could_not_get_socket_inodes     | Count of the number of times that ProcfsScraper was unable to get the socket inodes                 |
| procfs_could_not_open_fd_dir           | Count of the number of times that ProcfsScraper was unable to open /proc/{pid}/fd                   |
| procfs_could_not_open_pid_dir          | Count of the number of times that ProcfsScraper was unable to open /proc/{pid}                      |
| procfs_could_not_open_proc_dir         | Count of the number of times that ProcfsScraper was unable to open /proc                            |
| procfs_could_not_read_cmdline          | Count of the number of times that ProcfsScraper was unable to read /proc/{pid}/cmdline              |
| procfs_could_not_read_exe              | Count of the number of times that ProcfsScraper was unable to read /proc/{pid}/exe                  |
| event_timestamp_distant_past           | Count of the number of times that an event timestamp older than an hour is seen                     |
| event_timestamp_future                 | Count of the number of times that an event timestamp in the future is seen                          |

Note that the `[syscall]` suffix in a metric name means that it is instanciated for each syscall and direction individually.

Note that if ProcfsScraper is unable to open /proc it is not able to open any of the subdirectories, but only procfs_could_not_open_proc_dir will be incremented in that case.

### system_inspector timers per syscall

```
Component: system_inspector::Stats
Prometheus name: rox_collector_events_typed
Units: microseconds
```

For each syscall, and in each direction, the total time consumed by every step
("process", "parse") is available, as well as the computed average duration in
micro-second. These metrics are enabled via `ROX_COLLECTOR_ENABLE_DETAILED_METRICS` environment
variable.

```
rox_collector_event_times_us_total{event_dir="<",event_type="accept",step="process"} 45994
...
rox_collector_event_times_us_avg{event_dir="<",event_type="accept",step="process"} 3
```


### Process lineage statistics

```
Component: CollectorStats
Prometheus name: rox_collector_process_lineage_info
Units: bytes
```

- `lineage_avg_string_len`: overall average length of the lineage description string
- `std_dev`: standard deviation of the lineage description string length

### Connection statistics

Those metrics sample values regarding connections stored in the ConnectionTracker
at every reporting interval (=scrape interval), and over a sliding time window.

They can be configured using
[environment variables](references.md#environment-variables)(`ROX_COLLECTOR_CONNECTION_STATS*`).

Each metric keeps track of both incoming/outgoing direction, and private/public
peer location. Corresponding labels are added to the reported values.

#### Total number of known connections

```
Component: ConnectionTracker
Prometheus names: rox_connections_total
Units: count
```

This is the number of connections known to the ConnectionTracker during a reporting interval.

Example: `rox_connections_total{dir="in",peer="private",quantile="0.5"} 101`
means that 50% of the values for the number of connections are lower than 101 in the time window
(typically 1 hour). This specific entry reflects the connections received by the host (`in`), from
a private IP.

Example output:
```
# HELP rox_connections_total Amount of stored connections over time
# TYPE rox_connections_total summary
rox_connections_total_count{dir="out",peer="public"} 36
rox_connections_total_sum{dir="out",peer="public"} 18
rox_connections_total{dir="out",peer="public",quantile="0.5"} 0
rox_connections_total{dir="out",peer="public",quantile="0.9"} 1
rox_connections_total{dir="out",peer="public",quantile="0.95"} 3
rox_connections_total_count{dir="out",peer="private"} 36
rox_connections_total_sum{dir="out",peer="private"} 59537
rox_connections_total{dir="out",peer="private",quantile="0.5"} 1558
rox_connections_total{dir="out",peer="private",quantile="0.9"} 2067
rox_connections_total{dir="out",peer="private",quantile="0.95"} 2119
rox_connections_total_count{dir="in",peer="public"} 36
rox_connections_total_sum{dir="in",peer="public"} 0
rox_connections_total{dir="in",peer="public",quantile="0.5"} 0
rox_connections_total{dir="in",peer="public",quantile="0.9"} 0
rox_connections_total{dir="in",peer="public",quantile="0.95"} 0
rox_connections_total_count{dir="in",peer="private"} 36
rox_connections_total_sum{dir="in",peer="private"} 5009
rox_connections_total{dir="in",peer="private",quantile="0.5"} 101
rox_connections_total{dir="in",peer="private",quantile="0.9"} 179
rox_connections_total{dir="in",peer="private",quantile="0.95"} 180
```

#### Rate of connection creation

```
Component: ConnectionTracker
Prometheus names: rox_connections_rate
Units: connections per second
```

This is the rate of connections created during a reporting interval.

Example output:
```
# HELP rox_connections_rate Rate of connections over time
# TYPE rox_connections_rate summary
rox_connections_rate_count{dir="out",peer="public"} 35
rox_connections_rate_sum{dir="out",peer="public"} 0.06666667014360428
rox_connections_rate{dir="out",peer="public",quantile="0.5"} 0
rox_connections_rate{dir="out",peer="public",quantile="0.9"} 0
rox_connections_rate{dir="out",peer="public",quantile="0.95"} 0
rox_connections_rate_count{dir="out",peer="private"} 35
rox_connections_rate_sum{dir="out",peer="private"} 1947.28048324585
rox_connections_rate{dir="out",peer="private",quantile="0.5"} 51.43333435058594
rox_connections_rate{dir="out",peer="private",quantile="0.9"} 67.80000305175781
rox_connections_rate{dir="out",peer="private",quantile="0.95"} 69.53333282470703
rox_connections_rate_count{dir="in",peer="public"} 35
rox_connections_rate_sum{dir="in",peer="public"} 0
rox_connections_rate{dir="in",peer="public",quantile="0.5"} 0
rox_connections_rate{dir="in",peer="public",quantile="0.9"} 0
rox_connections_rate{dir="in",peer="public",quantile="0.95"} 0
rox_connections_rate_count{dir="in",peer="private"} 35
rox_connections_rate_sum{dir="in",peer="private"} 119.9425313472748
rox_connections_rate{dir="in",peer="private",quantile="0.5"} 2.17241382598877
rox_connections_rate{dir="in",peer="private",quantile="0.9"} 4.800000190734863
rox_connections_rate{dir="in",peer="private",quantile="0.95"} 4.833333492279053
```

## Troubleshooting using gperftools

Collector includes gperftools API for troubleshooting runtime performance, in
particular memory issues. The API endpoint is exposed on port `8080`, and
allows managing profiling status and fetch the result:

```
$ curl -X POST -d "on" collector:8080/profile/heap
# leave some time for gathering a profile
$ curl -X POST -d "off" collector:8080/profile/heap
# fetch the result
$ curl collector:8080/profile/heap
```

The resulting profile could be processed with `pprof` to get a human-readable
output with debugging symbols.

Collector also exposes a CPU profiler under `/profile/cpu`, which operates in
a very similar manner to the heap profiler.

---
**_NOTE_**: If the CPU profiler fails to start, make sure /var/profiles exists
in the collector container and is writable.

---

## Benchmark CI step

Whenever in doubt about performance implications of your changes, there is an
option to run a benchmark against the PR and compare the results with the known
baseline numbers. To do that, add a "run-benchmark" label to the PR. The
performance comparison will be reported via commentary to the PR with metrics
for CPU utilization and memory consumption. Along with the median values for
each resource a p-value sign will be reported, which could be interpreted as
how high are the chances that the observed difference is purely due to the
noise. The underlying mechanism is t-test, more than 80% probability it's just
a noise will result in green, less than that -- in red.

The benchmark will be conducted with two short simultaneous workloads. You can
find the configuration for them inside the berserker integration test image.
The baseline numbers are stored on GCS, and contains last 10 runs from the main
branch. The reporting filters out distinctly different results with small median
difference, to not bother without significant reasons, e.g. differences in CPU
utilization less than 1% and in memory less than 10 MiB are ignored. Keep in
mind that false-positives are definitely possible due to the noisiness of the
CI platform.

If for development purposes it's necessary to update the baseline from a PR,
not only just for the main branch, you can add "update-baseline" label.

## Introspection endpoints

Another method for troubleshooting collector during development cycles is to
use its introspection endpoints. These are REST like endpoints exposed on port
`8080` and provide some more insights into data being held by collector in
JSON format.

In order to enable these introspection endpoints, the
`ROX_COLLECTOR_INTROSPECTION_ENABLE` environment variable needs to be set to
`true`.

The endpoints should be reachable from within the k8s cluster the collector
daemonset is deployed, but you can also access it from your local host by
using port-forward:

```sh
$ kubectl -n stackrox port-forward ds/collector 8080:8080 &
[1] 64384
Forwarding from 127.0.0.1:8080 -> 8080
Forwarding from [::1]:8080 -> 8080
```

### Container metadata endpoint

This endpoint provides a way for users to query metadata of a given container
by its ID, querying the `/state/containers/{containerID}` endpoint. The
containerID argument needs to be provided in its short form (first 12
characters).

```sh
$ curl collector:8080/state/containers/01e8c0454972
```

In order for the metadata to be collected, the collector daemonset needs to be
edited for it to have access to the corresponding CRI socket on the system.
Since metadata collection is locked behind a feature flag, the
`ROX_COLLECTOR_RUNTIME_CONFIG_ENABLED` needs to be set to `true` as well.

```yaml
spec:
  template:
    spec:
      containers:
        ...
        env:
        - name: ROX_COLLECTOR_RUNTIME_CONFIG_ENABLED
          value: "true"
        ...
        volumeMounts:
        ...
        - mountPath: /host/run/containerd/containerd.sock
          mountPropagation: HostToContainer
          name: containerd-sock
        - mountPath: /host/run/crio/crio.sock
          mountPropagation: HostToContainer
          name: crio-sock
    ...
      volumes:
      ...
      - hostPath:
          path: /run/containerd/containerd.sock
        name: containerd-sock
      - hostPath:
          path: /run/crio/crio.sock
        name: crio-sock
```

Once edited, you can use the following command to extract the container IDs
from a given kubernetes object.

```sh
$ kubectl -n stackrox get pods -l "app=collector" -o json \
    | jq -r '.items[].status.containerStatuses[].containerID' \
    | sed -e 's#containerd://##' \
    | cut -c -12
01e8c0454972
80bfaff4d03b
```

You can then port-forward the collector daemonset to be able to query the
container metadata endpoint from your localhost. Alternatively, you can exec
into a pod with access to an HTTP tool in order to query the endpoint from
within the cluster itself.

```sh
$ curl "localhost:8080/state/containers/01e8c0454972"
Handling connection for 8080
{"container_id":"01e8c0454972","namespace":"stackrox"}
```

### Network endpoint

This endpoint provides visibility into connections and endpoints known to
collector.

The introspection API endpoints are as follows:
- `/state/network/endpoint` will return an array of the endpoints known
  to collector at that point.
- `/state/network/connection` is similar to the previous, but for connections.

Afterglow is not applied to the data returned by this API.

It is possible to filter the items returned per container_id by providing
a query parameter: `container=<container_id>`

By default, connections and endpoints are normalized in the result. It is
possible to disable normalization with a query parameter: `normalize=false`

Example of connection query, limited to container with identifier `c6f030bc4b42`
and without normalization :

```
$ curl "http://<collector>:8080/state/network/connection?container=c6f030bc4b42&normalize=false"
{
  "c6f030bc4b42" :
  [
    {
      "active" : true,
      "l4proto" : "TCP",
      "port" : 443,
      "to" : "10.96.0.1"
    }
  ]
}
```

### Runtime config endpoint
The runtime configuration can be obtained using
$ curl "http://<collector>:8080/state/runtime-config"

## Troubleshooting GRPC channel

In case of network connectivity issues it can be hard to figure out the reason
why Collector refuses to connect to Sensor. The reason for that is the
WaitForConnected GRPC method, which doesn't convey much information. A
workaround to get more information is to add the `GRPC_TRACE='*'` environment
variable to the DaemonSet, which will ask the GRPC library to trace all of its
components, e.g. producing something like this for situations where Sensor is
not available:

```
E0000 00:00:1739528892.222403      18 legacy_channel.cc:310] watch_completion_error: CANCELLED
I0000 00:00:1739528892.222495      18 completion_queue.cc:703] cq_end_op_for_next(cq=0x37a8eb40, tag=0x37a34300, error=UNKNOWN:Timed out waiting for connection state change {created_time:"2025-02-14T10:28:12.222428015+00:00"}, done=true, done_arg=0x37a62b40, storage=0x37a62b70)
I0000 00:00:1739528892.222536      18 completion_queue.cc:708] Operation failed: tag=0x37a34300, error=UNKNOWN:Timed out waiting for connection state change {created_time:"2025-02-14T10:28:12.222428015+00:00"}
```
