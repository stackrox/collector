package collector

import (
	"github.com/stackrox/collector/integration-tests/pkg/executor"
)

type StartupOptions struct {
	Mounts        map[string]string
	Env           map[string]string
	Config        map[string]any
	BootstrapOnly bool
}

type Manager interface {
	Setup(options *StartupOptions) error
	Launch() error
	TearDown() error
	IsRunning() (bool, error)
	ContainerID() executor.ContainerID
	TestName() string
	SetTestName(string)
	GetTestName() string
}

func New(e executor.Executor, name string) Manager {
	return NewDockerCollectorManager(e, name)
}
