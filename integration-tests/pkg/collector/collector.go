package collector

import (
	"path/filepath"
	"strings"

	"github.com/stackrox/collector/integration-tests/pkg/config"
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
	ContainerID() string
	TestName() string
}

func New(e executor.Executor, name string) Manager {
	k8sExec, ok := e.(*executor.K8sExecutor)
	if ok {
		return newK8sManager(*k8sExec, name)
	}
	return newDockerManager(e, name)
}

func getLogDirectory() string {
	return filepath.Join(".", "container-logs", config.VMInfo().Config, config.CollectionMethod())
}

func getLogFilename(m Manager) string {
	return strings.ReplaceAll(m.TestName(), "/", "_") + "-collector.log"
}
