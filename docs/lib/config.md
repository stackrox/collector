# Configuration System

The configuration system handles static startup options, environment variables, runtime updates, and platform-specific heuristics. Configuration flows through command-line parsing, environment variable resolution, and YAML file watching for dynamic updates.

## Static Configuration

### Argument Parsing

CollectorArgs is a singleton that uses optionparser for command-line arguments. The parse method processes --collector-config (JSON), --collection-method (ebpf/core_bpf), and --grpc-server. Each check* method validates inputs: checkCollectorConfig parses JSON via CollectorConfig::CheckConfiguration, checkCollectionMethod uses ParseCollectionMethod from CollectionMethod.h, and checkGRPCServer delegates to GRPC.cpp:CheckGrpcServer.

The singleton pattern with getInstance() allows validator callbacks to access the instance without passing pointers through optionparser's C-style API.

### Configuration Object

CollectorConfig starts with compile-time defaults: kScrapeInterval=30, kCollectionMethod=CORE_BPF, kSyscalls defines the base set (accept, connect, execve, etc.). InitCollectorConfig merges command-line args, environment variables, and JSON config. Processing order: log level first (to enable early logging), then scrape interval, collection method, and TLS paths.

Boolean flags like enable_processes_listening_on_ports_, import_users_, and track_send_recv_ read from BoolEnvVar instances. Network configuration includes ignored_l4proto_port_pairs_ (default: UDP:9), ignored_networks_ (169.254.0.0/16, fe80::/10), and non_aggregated_networks_. IPNet::parse converts CIDR strings to internal representation.

Sinsp buffer configuration defaults to DEFAULT_DRIVER_BUFFER_BYTES_DIM and DEFAULT_CPU_FOR_EACH_BUFFER from libscap. GetSinspBufferSize adjusts buffer dimensions: given sinsp_total_buffer_size_ (default 512MB) and CPU count, it calculates n_buffers = ceil(num_cpus / sinsp_cpu_per_buffer_), then max_buffer_size = ceil(total / n_buffers). The result must be a power of two >= 2^14 (16KB, two pages minimum) for ringbuffer alignment.

Connection stats use quantiles (default 0.50, 0.90, 0.95), error tolerance (0.01), and window size (60 seconds). These drive quantile estimation algorithms for network telemetry.

### Environment Variables

EnvVar.h templates provide lazy initialization with std::call_once. The ParseT functor converts strings: ParseBool accepts "true" case-insensitively, ParseStringList splits on commas, ParseInt/ParseFloat use standard library converters, ParsePath wraps std::filesystem::path.

Environment variables override defaults but are superseded by command-line arguments. For example, grpc_server first checks args->GRPCServer(), then the GRPC_SERVER env var. TLS paths check tls_certs_path first for a base directory, then individual overrides like tls_ca_path.

CollectorConfig:282-348 shows HandleAfterglowEnvVars and HandleConnectionStatsEnvVars parsing float/int/list types with try-catch for robustness. Invalid values log warnings and preserve defaults.

## Runtime Configuration

### YAML Parsing

ConfigLoader uses ParserYaml to convert YAML files into sensor::CollectorConfig protobufs. ParserYaml::Parse recursively walks protobuf descriptors, matching YAML fields to protobuf FieldDescriptors. read_camelcase_ mode converts between YAML camelCase and protobuf snake_case via SnakeCaseToCamel/CamelCaseToSnake.

Validation modes: STRICT requires all fields, PERMISSIVE allows missing/unknown fields, UNKNOWN_FIELDS_ONLY rejects extra fields but allows omissions. FindUnknownFields recursively scans YAML nodes to detect typos or deprecated fields.

ParseScalar handles primitive types via TryConvert<T>, wrapping YAML::Node::as<T>() to return std::variant<T, ParserError> instead of throwing. Enum parsing uppercases strings and uses EnumDescriptor::FindValueByName. ParseArray iterates sequences, calling ParseArrayInner for primitives or ParseArrayEnum for enums.

### File Watching

ConfigLoader::WatchFile uses Inotify to monitor /etc/stackrox/runtime_config.yaml (from ROX_COLLECTOR_CONFIG_PATH). Three watchers track: LOADER_PARENT_PATH for the parent directory (to catch file creation), LOADER_CONFIG_FILE for the config file itself, and LOADER_CONFIG_REALPATH for the symlink target if the config is a symlink.

HandleConfigDirectoryEvent detects IN_CREATE/IN_MOVED_TO to add file watchers, and IN_DELETE/IN_MOVED_FROM to reset runtime config. HandleConfigFileEvent responds to IN_MODIFY by reloading, and IN_MOVE_SELF/IN_DELETE_SELF by checking if the file reappeared (atomic writes via rename). HandleConfigRealpathEvent handles symlink target changes, re-pointing the watcher to the new target.

LoadConfiguration creates a default sensor::CollectorConfig with NewRuntimeConfig (setting max_connections_per_minute to kMaxConnectionsPerMinute), parses the YAML, and calls config_.SetRuntimeConfig with the result. The config is protected by a shared_mutex, with ReadLock for consumers and WriteLock for updates.

### Runtime Access

GetExternalIPsConf, MaxConnectionsPerMinute, and GetRuntimeConfigStr acquire ReadLock and check runtime_config_.has_value(). This pattern allows runtime_config_ to be absent (no YAML file) without affecting static configuration. ResetRuntimeConfig clears the optional, reverting to environment variables and static defaults.

CollectorRuntimeConfigInspector exposes runtime config via HTTP at /state/runtime-config. The handleGet method calls configToJson, which uses protobuf::util::MessageToJsonString with always_print_fields_with_no_presence to include zero values.

## Platform Heuristics

### Host Configuration

HostConfig provides per-host overrides discovered at runtime. SetCollectionMethod and SetNumPossibleCPUs populate values that supersede CollectorConfig when HasCollectionMethod returns true. HostHeuristics.cpp:ProcessHostHeuristics applies heuristics in order.

CollectionHeuristic validates eBPF support via HostInfo::HasEBPFSupport, BTF symbols, BPF ringbuffer support, and BPF tracing support. CORE_BPF requires all four; missing any results in CLOG(FATAL). EBPF mode checks if CORE_BPF is available and logs an informational message suggesting the upgrade.

DockerDesktopHeuristic fails fatally if HostInfo::IsDockerDesktop, since Docker Desktop doesn't support eBPF. PowerHeuristic checks for RHEL 8.6 on ppc64le with kernel <4.18.0-477, which lacks CORE_BPF support. CPUHeuristic queries HostInfo::NumPossibleCPU via libbpf_num_possible_cpus and stores the result for buffer size calculations.

### Host Information

HostInfo is a singleton providing lazy-initialized system data. GetKernelVersion parses uname.release (e.g., "4.18.0-372.el8.x86_64") into kernel/major/minor/build_id integers via regex. HasEBPFSupport returns true for kernel >=4.14 or RHEL 7.6 (3.10.0-957+).

HasBTFSymbols checks paths from libbpf's search order: /sys/kernel/btf/vmlinux (kernel-provided), /boot/vmlinux-*, /lib/modules/*/vmlinux-*, /usr/lib/debug paths. Uses faccessat with AT_EACCESS to respect permissions. Paths with .mounted=true prepend GetHostPath for containerized operation.

HasBPFRingBufferSupport calls libbpf_probe_bpf_map_type(BPF_MAP_TYPE_RINGBUF), checking the kernel's bpf() syscall response. HasBPFTracingSupport probes BPF_PROG_TYPE_TRACING. These probes execute quickly (single syscall) and avoid loading actual BPF programs. They run during heuristic evaluation, before attempting to load the CO-RE probe.

GetSecureBootStatus reads UEFI variables from /sys/firmware/efi/efivars/SecureBoot-* (5 bytes: 4-byte attributes + 1-byte status) or boot_params at offset 0x1EC for older kernels. Returns ENABLED/DISABLED/NOT_DETERMINED.

GetHostname checks NODE_HOSTNAME env var, then /etc/hostname and /proc/sys/kernel/hostname via GetHostnameFromFile. CLOG(FATAL) if none found. GetDistro/GetOSID/GetBuildID parse /etc/os-release or /usr/lib/os-release in KEY="VALUE" format via filterForKey.

### TLS Configuration

TlsConfig is a simple container for CA/cert/key paths. IsValid checks all three are non-empty. CollectorConfig:198-215 handles three sources: JSON tlsConfig with explicit paths, tls_certs_path base directory (defaults ca.pem/cert.pem/key.pem), or individual path env vars.

GRPC.cpp:24 uses ReadFileContents to load PEM strings into grpc::SslCredentialsOptions. Files are read once at startup rather than watched for changes, so certificate rotation requires collector restart.

## Configuration Lifecycle

1. CollectorArgs::parse processes command-line flags, validating and storing in singleton
2. CollectorConfig::InitCollectorConfig merges args, env vars, and applies defaults
3. ProcessHostHeuristics inspects HostInfo and sets HostConfig overrides
4. ConfigLoader::Start begins watching runtime config YAML in background thread
5. Runtime updates arrive via LoadConfiguration, acquiring WriteLock and calling SetRuntimeConfig
6. Readers acquire ReadLock and check runtime_config_.has_value() for optional overrides

This layering allows static configuration for kernel instrumentation while enabling dynamic network policy updates without restarting collector.
