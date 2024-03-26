package collector_manager

import "github.com/stackrox/collector/integration-tests/pkg/executor"

type CollectorStartupOptions struct {
	Mounts        map[string]string
	Env           map[string]string
	Config        map[string]any
	BootstrapOnly bool
}

type CollectorManager interface {
	Setup(options *CollectorStartupOptions) error
	Launch() error
	TearDown() error
	IsRunning() (bool, error)
	ContainerID() string
}

func New(e executor.Executor, name string) CollectorManager {
	_, ok := e.(*executor.K8sExecutor)
	if ok {
		return newK8sManager(e, name)
	}
	return newDockerManager(e, name)
}
