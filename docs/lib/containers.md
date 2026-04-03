# Container Abstraction

The container layer maps processes to container IDs, extracts Kubernetes metadata, and provides HTTP introspection of container state. This bridges kernel events (which track PIDs) to orchestrator concepts (pods, namespaces).

## Container Identification

### Container Engine

ContainerEngine (ContainerEngine.h:8) implements libsinsp::container_engine::container_engine_base. The resolve method receives a sinsp_threadinfo and queries_os_for_missing_info flag. It iterates tinfo->cgroups(), calling ExtractContainerIDFromCgroup on each cgroup path.

ExtractContainerIDFromCgroup (defined elsewhere in container_engine code) parses cgroup paths like /kubepods/besteffort/pod.../...container_id. When a container ID is found, it's assigned to tinfo->m_container_id and resolve returns true. No container ID means the process isn't containerized, returning false.

This engine integrates with libsinsp's container resolution pipeline. Multiple container engines can register; sinsp tries each until one succeeds. ContainerEngine runs early, using cgroup paths without querying container runtimes.

## Metadata Extraction

### Container Metadata

ContainerMetadata wraps sinsp and EventExtractor to provide container information. The constructor (ContainerMetadata.cpp:9) creates EventExtractor and calls Init(inspector), which sets up internal libsinsp pointers.

GetNamespace(sinsp_evt*) calls event_extractor_->get_k8s_namespace(event), which reads Kubernetes labels from the event's container. EventExtractor is defined in system-inspector/EventExtractor.h (not shown, but used here). The method returns the namespace string or empty if not available.

GetNamespace(container_id) queries libsinsp's container cache directly. inspector_->m_container_manager.get_containers() returns a map of container IDs to container info structs. The method looks up container_id, searches its m_labels map for "io.kubernetes.pod.namespace", and returns the value or empty string.

GetContainerLabel generalizes this pattern: given container_id and label key, it queries the container cache and returns the label value. This supports arbitrary label extraction beyond namespaces (e.g., pod name, deployment).

### Event Extractor

EventExtractor (used but not defined in shown files) provides accessors for event metadata. get_k8s_namespace reads labels added by libsinsp when it correlates events with container state. Init must be called with a sinsp instance to establish the connection.

The extractor abstracts libsinsp's internal structures. Rather than navigating sinsp_evt->threadinfo->container->labels, callers use get_k8s_namespace directly. This insulates collector code from libsinsp API changes.

## Introspection

### Container Info Inspector

ContainerInfoInspector handles HTTP GET requests at /state/containers/{id}. handleGet (ContainerInfoInspector.cpp:10) extracts the container ID from req_info->local_uri by finding the last slash and taking the substring after it. It validates length == 12 (standard short container ID).

The response builds a Json::Value with container_id and namespace fields. GetNamespace(container_id) queries ContainerMetadata. Json::FastWriter::write serializes to a compact string. The HTTP response is 200 OK with application/json content type.

Invalid container IDs return ClientError (defined in CivetWrapper, sends 400 Bad Request). Server errors like failed request parsing return ServerError (500 Internal Server Error). This provides debugging visibility into container state without restarting collector.

### CivetWrapper Integration

ContainerInfoInspector extends CivetWrapper (not shown), which extends CivetHandler. GetBaseRoute returns kBaseRoute = "/state/containers/". CivetServer uses this prefix to route URLs starting with /state/containers/ to this handler.

CollectorService registers the handler only when config.IsIntrospectionEnabled(). This protects internal state from exposure in production unless explicitly enabled. The handler is instantiated with container_metadata_inspector_, which is created from system_inspector_.GetContainerMetadataInspector().

## Container Lifecycle

Containers appear in libsinsp's cache when processes spawn or when existing containers are discovered during initial scan. The cache updates as libsinsp processes clone/execve events and reads cgroup changes.

ContainerMetadata reads this cache without modifying it. The cache is eventually consistent: a newly started container may not appear immediately in GetNamespace results. EventExtractor's event-driven path via get_k8s_namespace accesses fresh data directly from events.

GetContainerLabel can query any label, not just Kubernetes-specific ones. Docker labels, OCI labels, and custom annotations are all accessible via the m_labels map. This supports future extensions like querying pod UIDs or deployment names.

## Error Handling

GetNamespace returns empty strings rather than throwing exceptions. Callers check for emptiness and handle missing metadata gracefully. This aligns with collector's philosophy of best-effort data collection.

GetContainerLabel similarly returns empty for missing containers or labels. The container cache lookup uses find() and checks for end() before dereferencing. Missing labels also return empty.

ContainerInfoInspector validates container ID length but doesn't verify the container exists. GetNamespace will return empty for invalid IDs, resulting in valid JSON with an empty namespace field. This prevents errors from stopping introspection queries.

## Integration with System Inspector

system_inspector::Service (defined in system-inspector/Service.h) owns the ContainerMetadata instance and exposes it via GetContainerMetadataInspector(). This ensures ContainerMetadata and EventExtractor lifecycle match the sinsp instance.

ContainerMetadata's dependency on sinsp means it must be destroyed before sinsp. CollectorService's destruction order (system_inspector_.CleanUp last) ensures correct teardown.

NetworkSignalHandler and other components may also use ContainerMetadata to enrich events with Kubernetes context. The shared access model allows multiple readers without synchronization, as libsinsp's container cache is internally synchronized.
