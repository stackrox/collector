# Integration Tests

## Overview

Go-based test framework validating collector across platforms, kernels, and container runtimes. Tests simulate workloads with mock sensor (gRPC server) to verify process, network, and endpoint data capture.

**Location:** `integration-tests/`
**Framework:** testify/suite (26 test suites)
**Runtimes:** Docker, Podman, CRI-O, containerd

## Test Flow

1. Start mock sensor (gRPC on port 9999)
2. Launch collector container
3. Wait for health check
4. Run workload containers
5. Execute commands, generate events
6. Collect events from mock sensor
7. Verify expected events
8. Collect stats, teardown

## Core Test Suites

### Process and Execution

**ProcessNetworkTestSuite** (`process_network.go`)
Nginx container with curl client. Verifies process events (ls, nginx, sh, sleep), process lineage (awk→grep→sleep with parent info), network connections (server-side nginx, client-side curl TCP flows).

**SymbolicLinkProcessTestSuite** (`symlink_process.go`)
Process detection when executable invoked via symlink. Verifies name/path resolution.

**ThreadsTestSuite** (`threads.go`)
Multi-threaded process detection. Thread creation/termination events.

### Network

**ConnectionsAndEndpointsTestSuite** (`connections_and_endpoints.go`)
Normal ports (40, ephemeral), high/low mixing (40000, 10000), ephemeral server (60999), UDP with socket options (reuseaddr, fork). Verifies TCP/UDP tracking, endpoint detection, client/server roles, close timestamps.

**RepeatedNetworkFlowTestSuite** (`repeated_network_flow.go`)
Afterglow period testing. With afterglow (10s): connection deduplication. Zero afterglow: all connections separate. No afterglow flag: no coalescing. Parameters: AfterglowPeriod, ScrapeInterval, NumIter, SleepBetweenCurlTime.

**UdpNetworkFlow** (`udp_networkflow.go`)
UDP connection tracking. Skipped on fedora-coreos, rhel-8, rhcos, rhel-sap, s390x, ppc64le (ROX-27673).

**AsyncConnectionTestSuite** (`async_connections.go`)
Async connection tracking. Blocked connections (not reported by default), successful connections (always reported), connection status tracking disabled (all reported).

**SocatTestSuite** (`socat.go`)
Complex socket configurations using socat.

**DuplicateEndpointsTestSuite** (`duplicate_endpoints.go`)
Duplicate endpoint deduplication logic.

### Procfs and Scraping

**ProcfsScraperTestSuite** (`procfs_scraper.go`)
Scrape enabled: detects nginx on port 80 before collector started. Scrape disabled: no pre-existing endpoints. Feature flag disabled: endpoint detected but no originator process info. Verifies procfs scraper detects endpoints opened before collector, populates originator (process name, path, args).

**MissingProcScrapeTestSuite** (`missing_proc_scrape.go`)
Behavior when /proc entries missing/inaccessible. Local fake proc only (not K8s).

**ProcessListeningOnPortTestSuite** (`listening_ports.go`)
Endpoint detection for listening sockets, process association.

### Configuration and Runtime

**RuntimeConfigFileTestSuite** (`runtime_config_file.go`)
Runtime configuration reload without restart. Configuration changes detected via inotify, settings applied without restart (afterglow, scrape interval, network connection config). Flow: start collector with initial config, modify runtime config file, verify new settings, test network behavior.

**CollectorStartupTestSuite** (`collector_startup.go`)
Collector initialization. Health check endpoint available, self-check process executes.

**LogLevelTestSuite** (`log_level_endpoint.go`)
Dynamic log level via `/state/log-level` endpoint.

### Performance and Profiling

**BenchmarkCollectorTestSuite / BenchmarkBaselineTestSuite** (`benchmark.go`)
Measures overhead under load. Workloads: berserker/processes (short-lived processes), berserker/endpoints (many connections). Baseline: workload without collector. Collector: workload with collector. Metrics: CPU/memory (mean, stddev), duration.

Performance tools:
- COLLECTOR_PERF_COMMAND: Linux perf (e.g., `record -o /tmp/perf.data`)
- COLLECTOR_BPFTRACE_COMMAND: BPFtrace scripts (e.g., `/tools/collector-syscalls-count.bt`)
- COLLECTOR_BCC_COMMAND: BCC tools (e.g., `syscount --latency`)

**GperftoolsTestSuite** (`gperftools.go`)
Gperftools heap profiling (x86_64 only). CPU/heap profiler endpoints.

**PerfEventOpenTestSuite** (`perf_event_open.go`)
perf_event_open syscall handling, kernel perf event compatibility.

**PrometheusTestSuite** (`prometheus.go`)
Prometheus `/metrics` endpoint, collector statistics.

### API and Introspection

**HttpEndpointAvailabilityTestSuite** (`http_endpoint_availability.go`)
HTTP introspection API: `/ready`, `/state/network/connection`, `/state/network/endpoint`, `/state/log-level`. Requires ROX_COLLECTOR_INTROSPECTION_ENABLE=true.

**ImageLabelJSONTestSuite** (`image_json.go`)
Container image metadata extraction, Docker image labels/JSON parsing.

### Edge Cases

**ProcessesAndEndpointsTestSuite** (`processes_and_endpoints.go`)
Process/endpoint event correlation.

**RingBufferTestSuite** (`ringbuf.go`)
CO-RE BPF ring buffer functionality, event delivery.

### Kubernetes

**Location:** `suites/k8s/`
**Build tag:** `k8s`

**K8sNamespaceTestSuite** (`namespace.go`)
Kubernetes namespace isolation, multi-container pod visibility, service mesh scenarios.

**K8sConfigReloadTestSuite** (`config_reload.go`)
Configuration reload via ConfigMap updates, hot reload in K8s.

Running K8s tests:
```bash
cd integration-tests
CGO_ENABLED=0 GOOS=linux go test -tags k8s -c -o bin/collector-tests

cd ../ansible
ansible-playbook -i <inventory> -e '@k8s-tests.yml' k8s-integration-tests.yml
```

## Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| REMOTE_HOST_TYPE | Where tests run | local |
| VM_CONFIG | VM type/image (e.g., ubuntu.ubuntu-20.04) | - |
| COLLECTION_METHOD | Probe type | ebpf |
| COLLECTOR_IMAGE | Image to test | - |
| COLLECTOR_OFFLINE_MODE | Allow kernel object downloads | false |
| COLLECTOR_LOG_LEVEL | Log verbosity | debug |
| STOP_TIMEOUT | Container stop timeout (seconds) | 10 |
| COLLECTOR_PERF_COMMAND | Linux perf arguments | - |
| COLLECTOR_BPFTRACE_COMMAND | BPFtrace script/args | - |
| COLLECTOR_BCC_COMMAND | BCC tool/args | - |

Remote host:
- REMOTE_HOST_USER: SSH username
- REMOTE_HOST_ADDRESS: SSH host or GCP instance
- REMOTE_HOST_OPTIONS: SSH key path or GCP options

## VM Configurations

| VM Type | Example VM_CONFIG |
|---------|-------------------|
| cos | cos.cos-stable |
| rhel | rhel.rhel-8 |
| suse | suse.sles-15 |
| ubuntu-os | ubuntu-os.ubuntu-2204-lts |
| flatcar | flatcar.flatcar-stable |
| fedora-coreos | fedora-coreos.fedora-coreos-stable |
| garden-linux | garden-linux.garden-linux |

## Running Tests

Local development:
```bash
cd integration-tests
make TestProcessNetwork          # Single suite
make ci-integration-tests        # All tests
make ci-benchmarks              # Benchmarks
```

Dockerized:
```bash
make build-image                          # Build test image
make ci-integration-tests-dockerized      # Run in container
make TestProcessNetwork-dockerized        # Specific test
```

Remote VM:
```bash
cd ../ansible
VM_TYPE=rhel ansible-playbook -i dev integration-tests.yml

REMOTE_HOST_TYPE=gcloud \
VM_CONFIG=ubuntu.ubuntu-20.04 \
COLLECTOR_TEST=TestProcessNetwork \
ansible-playbook -i dev integration-tests.yml --tags run-tests
```

SSH remote:
```bash
REMOTE_HOST_TYPE=ssh \
REMOTE_HOST_USER=ec2-user \
REMOTE_HOST_ADDRESS=10.0.1.50 \
REMOTE_HOST_OPTIONS="-i ~/.ssh/my-key.pem" \
make TestProcessNetwork
```

## Mock Sensor

Simulates StackRox Sensor component. Located in `pkg/mock_sensor/`, port 9999, gRPC protocol, BoltDB in-memory storage.

API methods:
- PushSignals(): Receives process/network/endpoint events
- GetProcesses(): Query process events
- GetConnections(): Query connections
- GetEndpoints(): Query listening endpoints
- GetLineageInfo(): Query parent/child relationships

Verification helpers in test suites:
- ExpectProcesses(t, containerID, timeout, ProcessInfo{...})
- ExpectConnections(t, containerID, timeout, NetworkConnection{...})
- ExpectEndpoints(t, containerID, timeout, EndpointInfo{...})
- ExpectLineages(t, containerID, timeout, "bash", ProcessLineage{...})

## Test Infrastructure

Base suite: `IntegrationTestSuiteBase` provides lifecycle (StartCollector, StopCollector, RegisterCleanup), utilities (Executor, Collector, Sensor, execContainer, waitForContainerToExit, getIPAddress, getPorts), performance (StartContainerStats, SnapshotContainerStats, PrintContainerStats, WritePerfResults).

Container executor abstraction supports Docker, CRI-O, containerd via `Executor` interface (StartContainer, StopContainer, ExecContainer, GetContainerLogs, GetContainerStats, PullImage). Implementations: DockerExecutor (Docker CLI), CRIExecutor (CRI-O/containerd gRPC).

## Artifacts

Logs saved to `container-logs/<test-name>/`:
```
container-logs/
├── TestProcessNetwork/
│   ├── collector.log
│   ├── grpc-server.log
│   ├── nginx.log
│   └── nginx-curl.log
```

Performance data in `perf.json`:
```json
{
  "TestName": "TestBenchmarkCollector",
  "VmConfig": "ubuntu.ubuntu-20.04",
  "CollectionMethod": "ebpf",
  "Metrics": {
    "collector_cpu_mean": 12.5,
    "collector_mem_mean": 85.2
  }
}
```

JUnit reports: `make report` generates `integration-test-report.xml`.

## Troubleshooting

Test timeout: check collector logs, verify mock sensor received events, increase timeout.

Container fails: check image availability, review logs, verify kernel compatibility (eBPF >= 4.14).

Network events not captured: verify collection method, check probe loaded (lsmod/bpftool), review procfs scraper config.

Performance instability: increase berserker timeout, reduce concurrent connections, check resource contention.

Debug mode:
```bash
COLLECTOR_LOG_LEVEL=trace make TestProcessNetwork
CLEANUP_ON_FAIL=false make TestProcessNetwork
dlv test -- -test.run TestProcessNetwork
```

K8s debugging:
```bash
kubectl -n collector-tests logs collector
kubectl -n collector-tests describe pod collector
kubectl -n collector-tests get configmap runtime-config -o yaml
```

## Key Files

| File | Purpose |
|------|---------|
| integration_test.go | Main test registration |
| k8s_test.go | K8s test registration (tag: k8s) |
| benchmark_test.go | Benchmark registration (tag: bench) |
| Makefile | Test execution targets |
| suites/base.go | Base test suite utilities |
| pkg/mock_sensor/ | Mock sensor implementation |
| pkg/executor/ | Container runtime abstraction |
| pkg/collector/ | Collector container manager |

## CI Integration

CircleCI runs tests across platforms:
```yaml
integration-tests-rhel-8:
  environment:
    VM_TYPE: rhel
    VM_CONFIG: rhel.rhel-8
    COLLECTION_METHOD: ebpf
  steps:
    - checkout
    - run: cd ansible && make integration-tests
```

GitHub Actions for IBM Z/Power (ppc64le, s390x):
```yaml
jobs:
  test-s390x:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Run tests
        run: cd ansible && ansible-playbook -i ci integration-tests.yml
        env:
          IC_API_KEY: ${{ secrets.IBM_CLOUD_API_KEY }}
```

## References

- [Ansible Integration Tests](../ansible/README.md)
- [Collector Architecture](architecture.md)
- [Build System](build.md)
