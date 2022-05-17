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

* `ROX_NETWORK_GRAPH_PORTS`: Controls whether to retrieve TCP listening
sockets, while reading connection information from procfs. The default is true.

* `ROX_COLLECTOR_DISABLE_NETWORK_FLOWS`: Allows to disable processing of
network system call events and reading of connection information from procfs.
Mainly used in case of network-related performance degradation. The default is
false.

NOTE: Using environment variables is a preferred way of configuring Collector,
so if you're adding a new configuration knob, keep this in mind.

### Collector config

Collector configuration could be passed via `--collector-config` argument to
the binary and is represented as JSON string.

* `scrapeInterval`: Specifies how frequently network scraping is performed, in
seconds. The default value is 30 seconds.

* `turnOffScrape`: Turn off network scraping. The default is false.

* `logLevel`: Sets logging level. The default is INFO.

* `useChiselCache`: Whether to use cache for Chisel. For more details see
[Chisel](design-overview.md#Chisel) section.

### Other arguments

* `--collection-method`: Which technology to use for data gathering. Either
"ebpf" or "kernel_module".

* `--grpc-server`: GRPC server endpoint for Collector to communicate, in the
form "host:port".

* `--chisel`: Whether or not to use Chisel. Again, for more details see
[Chisel](design-overview.md#Chisel) section.

## Supported systems

## Glossary
