package config

const (
	CollectionMethodEBPF    = "ebpf"
	CollectionMethodCoreBPF = "core-bpf"

	runtimeDefaultCommand = "docker"
	runtimeDefaultSocket  = "/var/run/docker.sock"

	imageStoreLocation = "images.yml"
)

var (
	qa_tag            = ReadEnvVar(envQATag)
	collection_method = ReadEnvVarWithDefault(envCollectionMethod, CollectionMethodEBPF)

	image_store       *ImageStore
	collector_options *CollectorOptions
	runtime_options   *Runtime
	host_options      *Host
	vm_options        *VM
)

type Host struct {
	Kind    string
	User    string
	Address string
	Options string
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
