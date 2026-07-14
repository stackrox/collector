package collector

import (
	"github.com/stackrox/collector/integration-tests/pkg/executor"
)

// StartupOptions controls how a collector instance is configured for a
// test. Config entries are serialized to the collector YAML config file;
// Env entries become container environment variables. Keeping these
// separate matters because some knobs (e.g. afterglow, PLOP) are
// feature-flag env vars while others (e.g. scrapeInterval, turnOffScrape)
// are config-file settings — and they follow different code paths inside
// collector.
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

// New returns the default Manager implementation for the current
// environment. Today this is always Docker-based; the interface exists
// to support future Kubernetes-native test execution.
func New(e executor.Executor, name string) Manager {
	return NewDockerCollectorManager(e, name)
}
