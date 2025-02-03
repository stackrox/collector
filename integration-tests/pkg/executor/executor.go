package executor

import (
	"os/exec"

	"github.com/stackrox/collector/integration-tests/pkg/config"
)

// ContainerID is an identifier for a container. It can
// be a container name *OR* a hex ID
type ContainerID string

// Long returns the whole containerID as a string
func (c ContainerID) Long() string {
	return string(c)
}

// Short returns the first twelve character of a containerID
// to match the shortened IDs returned by docker.
func (c ContainerID) Short() string {
	return string(c)[:12]
}

type ContainerFilter struct {
	Id        ContainerID
	Namespace string
}

type ContainerLogs struct {
	Stdout string
	Stderr string
}

func (c *ContainerLogs) Empty() bool {
	return *c == (ContainerLogs{})
}

// Will return Stderr if it is not empty, otherwise it returns Stdout.
// Useful for accessing the logs for collector and container-stats
// that use a single stream.
func (l *ContainerLogs) GetSingleLog() string {
	if l.Stderr != "" {
		return l.Stderr
	}
	return l.Stdout
}

type Executor interface {
	PullImage(image string) error
	IsContainerRunning(container ContainerID) (bool, error)
	ContainerExists(filter ContainerFilter) (bool, error)
	ExitCode(filter ContainerFilter) (int, error)
	ExecContainer(container ContainerID, command []string) (string, error)
	KillContainer(container ContainerID) (string, error)
	RemoveContainer(filter ContainerFilter) (string, error)
	StopContainer(container ContainerID) (string, error)
	StartContainer(config config.ContainerStartConfig) (ContainerID, error)
	GetContainerHealthCheck(container ContainerID) (string, error)
	GetContainerIP(container ContainerID) (string, error)
	GetContainerLogs(container ContainerID) (ContainerLogs, error)
	CaptureLogs(testName string, container ContainerID) (string, error)
	GetContainerPort(container ContainerID) (string, error)
	IsContainerFoundFiltered(container ContainerID, filter string) (bool, error)
}

type CommandBuilder interface {
	ExecCommand(args ...string) *exec.Cmd
}

func New() (Executor, error) {
	return newDockerAPIExecutor()
}
