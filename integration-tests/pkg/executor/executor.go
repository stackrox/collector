package executor

import (
	"context"
	"os/exec"
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/config"
)

type ContainerFilter struct {
	Name      string
	Namespace string
}

type ContainerLogs struct {
	Stdout string
	Stderr string
}

type ContainerStat struct {
	Timestamp time.Time
	Id        string
	Name      string
	Mem       uint64
	Cpu       uint64
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

type ExecOptions struct {
	ContainerName string
	Command       []string
	Detach        bool
}

type Executor interface {
	PullImage(image string) error
	IsContainerRunning(container string) (bool, error)
	ContainerExists(filter ContainerFilter) (bool, error)
	ExitCode(filter ContainerFilter) (int, error)
	ExecContainer(opts *ExecOptions) (string, error)
	KillContainer(name string) (string, error)
	RemoveContainer(filter ContainerFilter) (string, error)
	StopContainer(name string) (string, error)
	StartContainer(config config.ContainerStartConfig) (string, error)
	GetContainerHealthCheck(containerID string) (string, error)
	GetContainerStats(ctx context.Context, containerID string) (*ContainerStat, error)
	GetContainerIP(containerID string) (string, error)
	GetContainerLogs(containerID string) (ContainerLogs, error)
	CaptureLogs(testName, containerName string) (string, error)
	GetContainerPorts(containerID string) ([]string, error)
	IsContainerFoundFiltered(containerID, filter string) (bool, error)
}

type CommandBuilder interface {
	ExecCommand(args ...string) *exec.Cmd
}

func New() (Executor, error) {
	command := config.RuntimeInfo().Command
	if command == "docker" || command == "podman" {
		return newDockerAPIExecutor()
	}
	return newCriExecutor()
}
