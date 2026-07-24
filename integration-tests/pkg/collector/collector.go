package collector

import (
	"github.com/stackrox/collector/integration-tests/pkg/executor"
)

// StartupOptions controls how a collector instance is configured for a test.
// Mounts maps host paths to container paths for volume binds. Config entries
// are serialized to the collector YAML config file. Env entries become
// container environment variables. Config and Env are separate because some
// knobs (e.g. afterglow, PLOP) are feature-flag env vars while others
// (e.g. scrapeInterval, turnOffScrape) are config-file settings — they
// follow different code paths inside collector.
type StartupOptions struct {
	Mounts        map[string]string
	Env           map[string]string
	Config        map[string]any
	BootstrapOnly bool
}

// Manager abstracts the collector container lifecycle so that the same
// test suites can run against different execution backends (e.g. Docker,
// Kubernetes) without changing test logic.
type Manager interface {
	Setup(options *StartupOptions) error
	Launch() error
	TearDown() error
	IsRunning() (bool, error)
	ContainerID() string
	TestName() string
	SetTestName(string)
	GetTestName() string
}

// New returns the default Docker-based Manager. The Manager interface
// allows the same test logic to work with different backends; execution
// is delegated to the supplied executor.
func New(e executor.Executor, name string) Manager {
	return NewDockerCollectorManager(e, name)
}
