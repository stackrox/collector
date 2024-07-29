# References

## Configuration options

### Environment variables

* `ENABLE_CORE_DUMP`: Controls whether a core dump file is created in the event
of a crash. Allowed values are true and false. The default is false.

* `ROX_ENABLE_AFTERGLOW`: Allows to enable afterglow functionality to reduce
networking usage. See the corresponding [Afterglow](design-overview.md#Afterglow)
section for more details. The default is true.

* `ROX_COLLECTOR_SET_CURL_VERBOSE`: Sets verbose mode and debug callback for
curl, when loading kernel objects. The default is false.

* `ROX_NETWORK_DROP_IGNORED`: Ignore connections with configured protocol and
port pairs (at the moment only `udp/9`). The default is true.

* `ROX_IGNORE_NETWORKS`: A coma-separated list of network prefixes to ignore.
Any connection with a remote peer matching this list will not be reported.
The default is `169.254.0.0/16,fe80::/10`

* `ROX_NON_AGGREGATED_NETWORKS`: A coma-separated list of network prefixes
indicating endpoints which should never be considered for aggregation.
This option can be useful when the CIDR blocks used for services or PODs are
not standard private subnets, as it will prevent Collector from handling them
as public IPs.

* `ROX_NETWORK_GRAPH_PORTS`: Controls whether to retrieve TCP listening
sockets, while reading connection information from procfs. The default is true.

* `ROX_COLLECTOR_DISABLE_NETWORK_FLOWS`: Allows to disable processing of
network system call events and reading of connection information from procfs.
Mainly used in case of network-related performance degradation. The default is
false.

* `ROX_PROCESSES_LISTENING_ON_PORT`: Instructs Collector to add information
about the originator process on all network listening-endpoint objects.
The default is false.

* `ROX_COLLECT_CONNECTION_STATUS`: Instruct Collector to track the network
connections status. With this enabled, advertising of asynchronous connections
will be postponed until their status is known and they are successful.
The default is true.

* `ROX_COLLECTOR_ENABLE_CONNECTION_STATS`: Instructs Collector to harvest
and publish metrics regarding the
[network connections](troubleshooting.md#connection-statistics) handled by the
connection tracker. The data is summarized in quantiles published by prometheus.
This is enabled by default.

  - `ROX_COLLECTOR_CONNECTION_STATS_QUANTILES`: a coma separated list of decimals
    defining the quantiles for all connection metrics. Default: `0.5,0.90,0.95`

  - `ROX_COLLECTOR_CONNECTION_STATS_ERROR`: the allowed error for the quantiles
    (used to aggregate observations). Default: `0.01`

  - `ROX_COLLECTOR_CONNECTION_STATS_WINDOW`: the length of the sliding time window
    in minutes. Default: `60`

* `ROX_COLLECTOR_SINSP_CPU_PER_BUFFER`: Allows to control how many sinsp
buffers are going to be allocated. The resulting number of buffers will be
calculated as the overall number of CPU cores available divided by this
value. The default value is 1, meaning one buffer for each CPU. The value 0 is
special, it instructs sinsp to allocate only one buffer no matter how many CPUs
are there. This parameter affects CO-RE BPF only.

* `ROX_COLLECTOR_SINSP_BUFFER_SIZE`: Specifies the size of a sinsp buffer in
bytes. The default value is 8MB. This value must be a power of 2, a multiple
of the system page size and greater than `2 * page_size`.

* `ROX_COLLECTOR_SINSP_TOTAL_BUFFER_SIZE`: Specifies the allowed total size of
all sinsp buffer in bytes. If the actual value will be larger than that due to
number of available CPUs, `ROX_COLLECTOR_SINSP_BUFFER_SIZE` will be adjusted to
match the limit. The default value is 512 MB and based on the default memory
limit specified for Collector DaemonSet in ACS.

* `ROX_COLLECTOR_SINSP_THREAD_CACHE_SIZE`: Puts upper limit on how many
thread info objects are going to be kept in memory. Since for process-based
workloads it's the main part of memory consumption, this value effectively
translates into the upper limit for memory usage. Note, that Falco puts it's
own upper limit on top of that, which is 2^17.

* `ROX_COLLECTOR_ENABLE_DETAILED_METRICS`: Specified whether to expose per-syscall metrics. This
information could be useful for troubleshooting, but under normal functioning
is quite verbose. The default is true.

* `ROX_COLLECTOR_INTROSPECTION_ENABLE`: Enable the introspection API and publish the
corresponding endpoints. With this API, it is possible to dump some of the
internal state of Collector. Refer to the
[troubleshooting](troubleshooting.md#introspection-endpoints) section for more details.
The default is false.

NOTE: Using environment variables is a preferred way of configuring Collector,
so if you're adding a new configuration knob, keep this in mind.

### Collector config

Collector configuration could be passed via `--collector-config` argument to
the binary and is represented as JSON string.

* `scrapeInterval`: Specifies how frequently network scraping is performed, in
seconds. The default value is 30 seconds.

* `turnOffScrape`: Turn off network scraping. The default is false.

* `logLevel`: Sets logging level. The default is INFO.

### Other arguments

* `--collection-method`: Which technology to use for data gathering. Either
"ebpf" or "core-bpf".

* `--grpc-server`: GRPC server endpoint for Collector to communicate, in the
form "host:port".
