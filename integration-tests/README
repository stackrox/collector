# Integration Tests

The integration tests for collector attempt to simulate a constrained operation of collector, along
with a mocked sensor (GRPC server) and some workload on the host. The mock GRPC server collects the 
events from collector, dumping them to Bolt DB, and they are then verified against what is expected
based on the workload that has run.

There is also a benchmark test which runs a phoronix container to test system performance under
a significant load, with and without collector running. This can give a ballpark idea of the 
overhead of running collector on a host, and can be used to compare the two collector methods.

Further performance tooling can be run alongside the benchmarks for more specific performance measurements. 
This tooling is detailed below.

## Environment Variables

The integration test behavior is controlled by a range of environment variables, detailed below. In particular,
the `REMOTE_HOST_TYPE` variable makes it possible to run the integration tests in a range of scenarios, from on
the local machine for development, or on gcloud or over SSH for remote testing in VMs.

| Variable Name            | Description                                                                             | Default (if applicable) |
| ------------------------ | --------------------------------------------------------------------------------------- | ----------------------- |
| `REMOTE_HOST_TYPE`       | the type of host to run the tests. One of (local, ssh, gcloud)                          | local                   |
| `VM_CONFIG`              | the description of the VM. e.g. ubuntu.ubuntu-20.04                                     | N/A                     |
| `COLLECTION_METHOD`      | the collection method for collector. One of (ebpf, kernel-module)                       | kernel-module           |
| `SSH_USER`               | if `REMOTE_HOST_TYPE` is `ssh`, the user to connect as.                                 | N/A                     |
| `SSH_ADDRESS`            | if `REMOTE_HOST_TYPE` is `ssh`, the address to connect to.                              | N/A                     |
| `SSH_KEY_PATH`           | if `REMOTE_HOST_TYPE` is `ssh`, the path to the private key to connect with.            | N/A                     |
| `GCLOUD_USER`            | if `REMOTE_HOST_TYPE` is `gcloud`, the user to connect as.                              | N/A                     |
| `GCLOUD_INSTANCE`        | if `REMOTE_HOST_TYPE` is `gcloud`, the name of the VM to connect to.                    | N/A                     |
| `GCLOUD_OPTIONS`         | if `REMOTE_HOST_TYPE` is `gcloud`, any additional options to pass to the gcloud command | N/A                     |
| `COLLECTOR_OFFLINE_MODE` | whether to allow kernel-object downloads. (true/false)                                  | false                   |
| `COLLECTOR_IMAGE`        | the name of the collector image to use.                                                 | N/A                     |
## Performance Measurement

To facilitate easier performance testing and measurement of collector whilst under
load, there are various ways to modify the behaviour of the benchmarks so that performance
tools are executed alongside them. The following environment variables provide this feature:

`COLLECTOR_BPFTRACE_COMMAND` - arguments to pass to a bpftrace command.

`COLLECTOR_PERF_COMMAND` - arguments to pass to a perf command.

`COLLECTOR_BCC_COMMAND` - arguments to pass to a BCC command.

`COLLECTOR_PERF_SKIP_INIT` - if set to `true`, do not run the init container.

To support these commands, the host is automatically updated with the necessary kernel
headers for the platform.

The tools are run in tool-specific docker containers on the host, and contain existing
scripts for use with the relevant tools. See the [perf containers documentation](container/perf/README)
for details.

`/tmp` is mounted in the containers to allow for extraction of data, if required.

### Measurement Examples

```bash
# Capture the count of all syscalls executed during the benchmark baseline
COLLECTOR_BPFTRACE_COMMAND="-e 'tracepoint:raw_syscalls:sys_enter { @syscalls = count(); }'" make baseline

# Run the collector-syscall-count tool
COLLECTOR_BPFTRACE_COMMAND='/tools/collector-syscalls-count.bt' make benchmark

# Run the syscount BCC tool, measuring syscall latency
COLLECTOR_BCC_COMMAND='syscount --latency' make benchmark

# Record perf events, writing /tmp
COLLECTOR_PERF_COMMAND='record -o /tmp/perf.data' make benchmark
```
