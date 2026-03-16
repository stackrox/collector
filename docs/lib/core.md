# Core Service Infrastructure

The core infrastructure manages collector's main loop, lifecycle, metrics collection, status reporting, and diagnostic endpoints. Components coordinate between configuration, kernel instrumentation, network monitoring, and sensor communication.

## Service Management

### Collector Service

CollectorService orchestrates all subsystems. The constructor (CollectorService.cpp:27) initializes system_inspector_ with config_, creates ConnectionTracker unless network flows are disabled, and sets up NetworkStatusNotifier linking conn_tracker_ and system_inspector_. NetworkSignalHandler attaches to system_inspector_ to process network events.

Civetweb handlers register at construction: GetStatus for /ready, LogLevelHandler for runtime log control, ProfilerHandler for CPU profiling. When introspection is enabled, ContainerInfoInspector (/state/containers/), NetworkStatusInspector, and CollectorConfigInspector expose internal state. Prometheus exposer binds to port 9090 and registers the exporter's registry.

RunForever (CollectorService.cpp:94) starts ConfigLoader, NetworkStatusNotifier, and CollectorStatsExporter. system_inspector_.Start launches the kernel event processing thread. The main loop calls system_inspector_.Run(*control_) until control_ transitions to STOP_COLLECTOR. On shutdown, components stop in reverse order: ConfigLoader, NetworkStatusNotifier, exporter, then system_inspector_.CleanUp.

InitKernel calls system_inspector_.InitKernel(config_), which selects and loads the CO-RE BPF probe. Startup diagnostics track driver availability and success for the diagnostic log in Diagnostics.h:StartupDiagnostics.

WaitForGRPCServer blocks until config_.grpc_channel reaches READY state, using an interrupt lambda that checks control_->load(). This ensures sensor connectivity before collecting events.

### Lifecycle Control

Control.h defines ControlValue enum: RUN continues operation, STOP_COLLECTOR initiates shutdown. The main function passes std::atomic<ControlValue> and std::atomic<int> signum to CollectorService, allowing signal handlers to trigger graceful termination.

system_inspector_.Run receives the control atomic and polls it during event processing. When STOP_COLLECTOR is detected, Run returns, unwinding the RunForever loop. The signum atomic preserves which signal triggered shutdown for diagnostic logging.

### Stoppable Threads

StoppableThread wraps std::thread with cancellation via pipe and condition variable. prepareStart creates a pipe, storing descriptors in stop_pipe_[]. Start launches the thread and stores it in thread_. The stop_fd() accessor returns stop_pipe_[0] for select/poll integration.

Stop sets should_stop_ atomic, notifies stop_cond_, closes the write end of the pipe (triggering POLLHUP on read end), then joins the thread. PauseUntil waits on stop_cond_ with a deadline, returning false if the deadline expires (continue operation) or true if should_stop() becomes true (shutdown).

ConfigLoader, SignalServiceClient, CollectorStatsExporter, and NetworkStatusNotifier use StoppableThread for background tasks. Threads check should_stop() in their loops and include stop_fd() in poll sets to wake immediately on cancellation.

## Metrics and Statistics

### Collector Stats

CollectorStats is a singleton with atomic counters and timers. TimerType enum includes net_scrape_read, net_scrape_update, process_info_wait. CounterType includes net_conn_updates, process_lineage_counts, procfs_* error counters. SCOPED_TIMER(index) creates an RAII timer that records duration on destruction. COUNTER_INC/COUNTER_SET macros modify atomics without locking.

The singleton pattern with GetOrCreate ensures static initialization before main. Reset zeroes all counters, used in tests. Array indexing by enum provides O(1) access for high-frequency operations.

### Stats Exporter

CollectorStatsExporter runs a background thread that polls CollectorStats and system_inspector_ every 5 seconds. The run method creates Prometheus gauges for rox_collector_events (kernel events, drops, preemptions), rox_collector_timers (per-timer events/total/avg), and rox_collector_counters.

When enable_detailed_metrics_ is true, it builds rox_collector_events_typed and rox_collector_event_times_* families with labels for event_type and event_dir (enter/exit). The loop checks config_->Syscalls() to filter events, only creating metrics for enabled syscalls.

Process lineage statistics compute average, standard deviation, and string length from process_lineage_total/process_lineage_sqr_total/process_lineage_string_total counters. Variance calculation: std_dev = sqrt((sqr_avg) - (avg)^2).

GetRegistry returns the prometheus::Registry shared_ptr for registration with the Exposer. The main loop creates prometheus::Exposer with port 9090 and calls RegisterCollectable.

### Event Statistics

system_inspector::Stats (from SystemInspector.h) provides kernel-level counters: nEvents, nDrops, nDropsBuffer, nDropsThreadCache, nPreemptions, nThreadCacheSize. GetStats populates this structure from libsinsp metrics.

nUserspaceEvents[PPM_EVENT_MAX] counts events processed in userspace by type. event_parse_micros and event_process_micros track time spent parsing events from kernel and processing them through handlers. These arrays align with PPM_EVENT_MAX from ppm_events_public.h.

Process event counters track nProcessSent, nProcessSendFailures, nProcessResolutionFailuresByEvt (missing process info during event), nProcessResolutionFailuresByTinfo (missing threadinfo). nGRPCSendFailures counts signal stream write errors.

## Status and Diagnostics

### Health Endpoint

GetStatus implements /ready for Kubernetes liveness/readiness probes. handleGet calls system_inspector_->GetStats(&stats), returning 503 Service Unavailable if the inspector isn't ready. Success returns 200 with JSON: hostname from HostInfo::GetHostname, events/drops breakdown, preemptions count.

The drops object separates total (nDrops), ringbuffer (nDropsBuffer), and threadcache (nDropsThreadCache). This granularity helps diagnose whether drops occur in kernel ringbuffer overflow or userspace cache eviction.

### Introspection Endpoints

ContainerInfoInspector parses container IDs from URLs like /state/containers/{id} (12 hex chars). It queries ContainerMetadata::GetNamespace, which uses EventExtractor to read Kubernetes labels from libsinsp's container cache. The response includes container_id and namespace as JSON.

NetworkStatusInspector (referenced but not defined in provided files) exposes ConnTracker state. CollectorConfigInspector converts runtime_config_ to JSON via protobuf::util::MessageToJsonString, allowing runtime inspection of YAML-based configuration.

CivetWrapper.h (not shown) provides a base class for HTTP handlers with GetBaseRoute. CollectorService::civet_endpoints_ stores unique_ptrs and calls server_.addHandler(endpoint->GetBaseRoute(), endpoint.get()) to register routes.

### Startup Diagnostics

StartupDiagnostics::GetInstance singleton accumulates initialization info. ConnectedToSensor records GRPC connection success. DriverAvailable, DriverSuccess, and DriverUnavailable track kernel driver loading attempts. Log dumps the accumulated state with connection status and driver candidates.

The diagnostic log appears once during startup, providing a snapshot of configuration and initialization outcomes for troubleshooting.

## Integration Patterns

### Thread Coordination

CollectorService owns all threads: system_inspector_ internal thread, ConfigLoader thread_, NetworkStatusNotifier threads, CollectorStatsExporter thread_. Destruction order matters: network components stop before system_inspector_.CleanUp to avoid using freed kernel state.

The control atomic provides single-bit signaling. More complex coordination uses dedicated condition variables (e.g., SignalServiceClient::stream_interrupted_) or combines control checks with component-specific atomics.

### Shared State Access

config_ uses shared_mutex: readers acquire ReadLock, writers acquire WriteLock. This allows many threads to read static config while ConfigLoader updates runtime_config_ occasionally. GetExternalIPsConf, MaxConnectionsPerMinute, GetRuntimeConfigStr follow this pattern.

ConnTracker (referenced in CollectorService) is shared via shared_ptr across NetworkStatusNotifier and NetworkSignalHandler. This shared ownership ensures the tracker outlives handler processing.

system_inspector_ is owned by CollectorService and accessed by GetStatus and CollectorStatsExporter via raw pointer. The service's lifetime guarantees these dependencies, so shared_ptr overhead is avoided.

### Error Propagation

Initialization failures use CLOG(FATAL), terminating the process. Runtime errors like GRPC stream failures log errors and set state (stream_active_=false) to trigger reconnection. The control atomic allows orderly shutdown rather than process exit on transient errors.

StoppableThread::prepareStart returns false if already running, and doStart always succeeds. Callers check the return value to avoid starting multiple threads. Stop is idempotent: closing an already-closed pipe continues, join blocks until the thread exits.

CollectorStatsExporter::start returns false if the thread is already running, true on success. This boolean return allows callers to handle the unlikely start failure without exceptions.

### Configuration Flow

Static config flows: CollectorArgs → CollectorConfig::InitCollectorConfig → HostHeuristics → CollectorService constructor. Runtime config flows: YAML file → ConfigLoader inotify → ParserYaml → CollectorConfig::SetRuntimeConfig. Readers check runtime_config_.has_value() to distinguish static vs. dynamic values.

This separation allows kernel configuration (syscalls, buffer sizes) to remain static while network policies (max_connections_per_minute, external IPs) update dynamically.
