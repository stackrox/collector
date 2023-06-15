package config

const (
	CollectionMethodEBPF    = "ebpf"
	CollectionMethodCoreBPF = "core-bpf"

	runtimeDefaultCommand = "docker"
	runtimeDefaultSocket  = "/var/run/docker.sock"

	imageStoreLocation = "images.yml"

	// defaultStopTimeoutSeconds is the amount of time to wait for a container
	// to stop before forcibly killing it. It needs to be a string because it
	// is passed directly to the docker command via the executor.
	//
	// 10 seconds is the default for docker stop when not providing a timeout
	// argument. It is kept the same here to avoid changing behavior by default.
	defaultStopTimeoutSeconds = "10"
)

var (
	qa_tag            = ReadEnvVar(envQATag)
	collection_method = ReadEnvVarWithDefault(envCollectionMethod, CollectionMethodEBPF)
	stop_timeout      = ReadEnvVarWithDefault(envStopTimeout, defaultStopTimeoutSeconds)

	image_store       *ImageStore
	collector_options *CollectorOptions
	runtime_options   *Runtime
	host_options      *Host
	vm_options        *VM
	benchmarks        *Benchmarks
)

type Host struct {
	Kind    string
	User    string
	Address string
	Options string
}

func (h *Host) IsLocal() bool {
	return h.Kind == "local"
}

type VM struct {
	InstanceType string
	Config       string
}

type Runtime struct {
	Command   string
	Socket    string
	RunAsRoot bool
}

type CollectorOptions struct {
	LogLevel     string
	Offline      bool
	PreArguments string
}

type Benchmarks struct {
	BccCommand      string
	BpftraceCommand string
	PerfCommand     string
	SkipInit        bool
}

func Images() *ImageStore {
	if image_store == nil {
		var err error
		image_store, err = loadImageStore(imageStoreLocation)
		if err != nil {
			// there is little we can do if we're unable to
			// load the image store, so simply panic
			panic(err)
		}
	}
	return image_store
}

func CollectionMethod() string {
	return collection_method
}

func StopTimeout() string {
	return stop_timeout
}

func HostInfo() *Host {
	if host_options == nil {
		host_options = &Host{
			Kind:    ReadEnvVarWithDefault(envHostType, "default"),
			User:    ReadEnvVar(envHostUser),
			Address: ReadEnvVar(envHostAddress),
			Options: ReadEnvVar(envHostOptions),
		}
	}

	return host_options
}

func VMInfo() *VM {
	if vm_options == nil {
		vm_options = &VM{
			InstanceType: ReadEnvVarWithDefault(envVMInstanceType, "default"),
			Config:       ReadEnvVar(envVMConfig),
		}
	}
	return vm_options
}

func RuntimeInfo() *Runtime {
	if runtime_options == nil {
		runtime_options = &Runtime{
			Command:   ReadEnvVarWithDefault(envRuntimeCommand, runtimeDefaultCommand),
			Socket:    ReadEnvVarWithDefault(envRuntimeSocket, runtimeDefaultSocket),
			RunAsRoot: ReadBoolEnvVar(envRuntimeAsRoot),
		}
	}
	return runtime_options
}

func CollectorInfo() *CollectorOptions {
	if collector_options == nil {
		collector_options = &CollectorOptions{
			LogLevel:     ReadEnvVarWithDefault(envCollectorLogLevel, "debug"),
			Offline:      ReadBoolEnvVar(envCollectorOfflineMode),
			PreArguments: ReadEnvVar(envCollectorPreArguments),
		}
	}
	return collector_options
}

func BenchmarksInfo() *Benchmarks {
	if benchmarks == nil {
		benchmarks = &Benchmarks{
			BccCommand:      ReadEnvVar(envBccCommand),
			BpftraceCommand: ReadEnvVar(envBpftraceCommand),
			PerfCommand:     ReadEnvVar(envPerfCommand),
			SkipInit:        ReadBoolEnvVar(envSkipHeadersInit),
		}
	}
	return benchmarks
}
