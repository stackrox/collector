# System Inspection Layer

SystemInspector is the critical abstraction boundary between collector's userspace logic and CO-RE BPF kernel instrumentation. It provides event streaming, statistics, and lifecycle management while hiding all kernel interaction details.

## Abstraction Boundary

SystemInspector (defined in system-inspector/SystemInspector.h, not shown in full) encapsulates libsinsp and the BPF probe. Collector components interact solely through SystemInspector's interface: no direct access to sinsp objects, BPF maps, or kernel data structures. This boundary enforces separation between event processing (above the line) and kernel instrumentation (below the line).

The interface exposes: event iteration via Run, statistics via GetStats, lifecycle control via Start/CleanUp/InitKernel, and handler registration via AddSignalHandler. All kernel complexity--CO-RE relocations, BPF program loading, ringbuffer management, threadinfo caching--remains hidden.

## Kernel Driver Management

### Driver Selection

KernelDriver.h defines IKernelDriver interface with Setup(config, inspector). KernelDriverCOREEBPF implements this for CO-RE BPF. Setup converts config.Syscalls() text names to ppm_sc_code syscall codes via EventNames::GetEventIDs and EventNames::GetEventSyscallID.

GetSyscallList builds an unordered_set<ppm_sc_code> from the configured syscalls. For each syscall string (e.g., "connect"), it looks up enter and exit event codes, retrieves the syscall ID from g_syscall_table (external libscap array), and inserts the ppm_sc_code. PPM_SC_SCHED_PROCESS_EXIT and PPM_SC_SCHED_SWITCH are added unconditionally--procexit controls threadinfo cache size, sched_switch improves process tracking reliability.

Setup calls inspector.open_modern_bpf(buffer_size, cpu_per_buffer, true, ppm_sc). The true parameter enables online snapshots. buffer_size comes from config.GetSinspBufferSize(), which may adjust based on total_buffer_size and CPU count. cpu_per_buffer determines how many CPUs share a ringbuffer (e.g., 2 means one buffer per 2 CPUs).

open_modern_bpf loads the CO-RE BPF object, attaches tracepoints, allocates ringbuffers, and starts event capture. Failures throw sinsp_exception, propagating to InitKernel's caller.

### Event Names

EventNames singleton maps between string names, event IDs (ppm_event_code), and syscall IDs. The constructor (EventNames.cpp:13) iterates g_event_info[PPM_EVENT_MAX] from libscap, populating names_by_id_ and events_by_name_. Each event has enter and exit variants; events_by_name_["connect"] includes both PPME_SOCKET_CONNECT_E and PPME_SOCKET_CONNECT_X.

Event names support directionality: "connect>" maps only to enter, "connect<" only to exit. This allows config.Syscalls() to specify directional filtering if needed, though current usage includes both directions.

syscall_by_id_ maps ppm_event_code to g_syscall_table index. This enables GetEventSyscallID to convert event IDs (which the kernel uses) to ppm_sc_code (which Setup needs). The indirection handles events that don't correspond to syscalls (e.g., procexit).

## Event Processing

### Event Extraction

EventExtractor wraps sinsp_evt accessors. Init(inspector) stores a sinsp pointer. Methods like get_k8s_namespace read labels from the event's associated container. The extractor hides sinsp_evt internal structure, providing a stable API for event attribute access.

Event iteration happens in system_inspector::Service::Run (not shown). It calls inspector_.next(&evt) in a loop, checks evt type, and dispatches to registered signal handlers. Each handler processes events of interest, extracting data via EventExtractor.

### Event Map

EventMap<T> (EventMap.h:12) is a template providing per-event-type storage. It wraps std::array<T, PPM_EVENT_MAX>, allowing indexed access by ppm_event_code. The Set(name, value) method uses EventNames::GetEventIDs to find all event codes matching the name and sets their values.

This supports configuring behavior per event type. For example, EventMap<bool> could enable/disable processing for specific syscalls. The template accepts any type T, including function pointers or strategy objects.

Construction accepts initializer_list<pair<string, T>> for declarative initialization: EventMap<int> map{{"connect", 42}, {"accept", 99}}. This sets both enter and exit events for each syscall.

### Host Heuristics

HostHeuristics.cpp:105 applies platform-specific configuration adjustments. ProcessHostHeuristics creates HostConfig and applies heuristics in sequence: CollectionHeuristic, DockerDesktopHeuristic, PowerHeuristic, CPUHeuristic.

CollectionHeuristic validates prerequisites: HasEBPFSupport fails fatally if the kernel is too old (except RHEL 7.6). For CORE_BPF, it checks HasBTFSymbols (vmlinux or BTF sysfs), HasBPFRingBufferSupport (BPF_MAP_TYPE_RINGBUF), and HasBPFTracingSupport (BPF_PROG_TYPE_TRACING). Missing any requirement is fatal. For EBPF mode, it checks if CORE_BPF is available and logs a recommendation.

DockerDesktopHeuristic detects Docker Desktop via HostInfo::IsDockerDesktop and fails--Docker Desktop doesn't support eBPF. PowerHeuristic checks ppc64le machines for RHEL 8.6 kernel <4.18.0-477, which lacks CORE_BPF. CPUHeuristic queries NumPossibleCPU and stores it in HostConfig for buffer calculations.

Heuristics run during CollectorConfig::InitCollectorConfig:279, after static config is loaded but before kernel initialization. This allows failing fast with clear error messages rather than obscure kernel errors.

## Host Information

### Kernel Version

HostInfo::GetKernelVersion checks the KERNEL_VERSION environment variable, then calls uname(2). KernelVersion::FromHost parses the release string with regex: `^(\d+)\.(\d+)\.(\d+)(-(\d+))?.*`. This extracts kernel (4), major (18), minor (0), and optional build_id (372 for "4.18.0-372.el8.x86_64").

HasEBPFSupport returns true for kernel >=4.14. Special case: RHEL 7.6 (3.10.0-957+) backports eBPF, detected via isRHEL76 which checks os_id=="rhel" or "centos", ".el7." substring, and build_id >= MIN_RHEL_BUILD_ID (957).

HasSecureBootParam checks kernel >=4.11, when boot_params gained the secure_boot field (commit de8cb458625c). This determines whether to read UEFI variables or boot_params.

### BTF Detection

HasBTFSymbols (HostInfo.cpp:206) searches paths from libbpf: /sys/kernel/btf/vmlinux, /boot/vmlinux-{release}, /lib/modules/{release}/vmlinux-{release}, /usr/lib/debug paths. snprintf formats the release into the path template. Paths with .mounted=true call GetHostPath to handle containerized execution.

faccessat with AT_EACCESS checks read permission. ENOTDIR or ENOENT means the file doesn't exist; other errors log warnings. First accessible file returns true. This search order prioritizes kernel-provided BTF (sysfs) over vmlinux files, which may be compressed or debug-only.

### BPF Capability Probing

HasBPFRingBufferSupport calls libbpf_probe_bpf_map_type(BPF_MAP_TYPE_RINGBUF, NULL). This issues a bpf() syscall with BPF_MAP_CREATE to check if the kernel supports the map type. Return 0 means unsupported, <0 means probe failed (assume supported), >0 means supported.

HasBPFTracingSupport probes BPF_PROG_TYPE_TRACING similarly. These probes execute quickly (single syscall) and avoid loading actual BPF programs. They run during heuristic evaluation, before attempting to load the CO-RE probe.

### Secure Boot

GetSecureBootStatus caches results in secure_boot_status_. For kernels >=4.11, GetSecureBootFromParams reads /sys/kernel/boot_params/data at SECURE_BOOT_OFFSET (0x1EC). The byte value maps to enum: 0=unset, 1=not determined, 2=disabled, 3=enabled.

For older kernels, GetSecureBootFromVars scans /sys/firmware/efi/efivars for "SecureBoot-*" files. EFI variables have 4 bytes of attributes plus the value. Reading 5 bytes gets both; byte 4 is the status (0 or 1). The UEFI spec defines this format.

IsUEFI checks for /sys/firmware/efi directory. If it exists, the system booted via UEFI; otherwise, legacy BIOS. This informs whether to attempt SecureBoot detection.

### CPU Topology

NumPossibleCPU wraps libbpf_num_possible_cpus, which reads /sys/devices/system/cpu/possible (e.g., "0-127"). This counts CPUs that can be onlined, including offline cores. The count determines ringbuffer allocation: n_buffers = ceil(num_cpus / cpu_per_buffer).

### OS Detection

GetOSReleaseValue reads /etc/os-release or /usr/lib/os-release (freedesktop standard). filterForKey parses KEY="VALUE" lines, stripping quotes. GetDistro reads PRETTY_NAME ("Ubuntu 22.04.1 LTS"). GetOSID reads ID ("ubuntu"). GetBuildID reads BUILD_ID (CentOS/RHEL version).

IsDockerDesktop checks PRETTY_NAME == "Docker Desktop". IsUbuntu checks ID == "ubuntu". IsRHEL76 checks ID in {rhel, centos}, ".el7." substring, and build_id >=957. IsMinikube checks hostname == "minikube". GetMinikubeVersion parses /etc/VERSION with regex `v\d+\.\d+\.\d+`.

## Statistics

### Stats Structure

system_inspector::Stats (referenced in GetStatus.cpp:18) aggregates metrics: nEvents (total kernel events), nDrops (lost events), nDropsBuffer (ringbuffer full), nDropsThreadCache (cache eviction), nPreemptions (userspace preempted during event read), nThreadCacheSize (current threadinfo count).

nUserspaceEvents[PPM_EVENT_MAX] counts events by type after kernel. event_parse_micros[PPM_EVENT_MAX] measures time parsing each event from binary. event_process_micros[PPM_EVENT_MAX] measures handler processing time. These arrays enable per-syscall performance analysis.

nProcessSent, nProcessSendFailures, nProcessResolutionFailuresByEvt, nProcessResolutionFailuresByTinfo track process signal generation. nGRPCSendFailures counts stream write errors. These counters flow to CollectorStatsExporter for Prometheus metrics.

### Event Timing

When enable_detailed_metrics_ is true, CollectorStatsExporter creates rox_collector_event_times_us_total and _avg metrics for each enabled syscall. Labels distinguish event_type (connect, accept) and event_dir (>, <). This granularity identifies slow syscalls or handler bottlenecks.

Parse time includes reading from ringbuffer and deserializing ppm_event structures. Process time includes running handlers and updating internal state. High parse time suggests ringbuffer contention or large event payloads. High process time suggests handler inefficiency.

## Lifecycle

InitKernel selects KernelDriverCOREEBPF, calls Setup, and logs driver success to StartupDiagnostics. Start launches the event loop thread. Run polls events and dispatches to handlers until control flag transitions to STOP_COLLECTOR. CleanUp closes the inspector, which unmaps ringbuffers and detaches BPF programs.

The destructor order matters: handlers must finish before sinsp closes. system_inspector_ lives in CollectorService, destroyed after networking and exporter threads stop. This ensures no handler callbacks fire after subsystems are torn down.

AddSignalHandler registers components that process specific event types. NetworkSignalHandler handles connect, accept, close for network tracking. ProcessSignalHandler (not shown) handles execve, procexit for process lineage. Handlers implement a common interface and receive events via callbacks.

## Integration Points

GetStats provides snapshot of kernel metrics for /ready health checks and Prometheus scraping. The method doesn't block event processing; it reads atomic counters or locked structures briefly.

GetContainerMetadataInspector returns a shared_ptr to ContainerMetadata, allowing HTTP handlers to query container state. The metadata lives as long as system_inspector_, tying its lifecycle to sinsp.

GetUserspaceStats (referenced in NetworkSignalHandler) exposes userspace-specific counters not available from libsinsp. This allows NetworkSignalHandler to track network-specific failures separately from generic event stats.

The inspector thread owns the event loop. Other threads interact via thread-safe methods: GetStats uses atomics, AddSignalHandler mutates before Start, Run blocks the calling thread. This single-threaded event model simplifies handler implementation--no locks needed within handler callbacks.
