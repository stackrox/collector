package executor

import (
	"os/exec"

	"github.com/stackrox/collector/integration-tests/pkg/config"
)

type ContainerFilter struct {
	Name      string
	Namespace string
}

type Executor interface {
	CopyFromHost(src string, dst string) (string, error)
	PullImage(image string) error
	IsContainerRunning(container string) (bool, error)
	ContainerExists(filter ContainerFilter) (bool, error)
	ContainerID(filter ContainerFilter) string
	ExitCode(filter ContainerFilter) (int, error)
	Exec(args ...string) (string, error)
	ExecWithErrorCheck(errCheckFn func(string, error) error, args ...string) (string, error)
	ExecWithStdin(pipedContent string, args ...string) (string, error)
	ExecWithoutRetry(args ...string) (string, error)
	KillContainer(name string) (string, error)
	RemoveContainer(filter ContainerFilter) (string, error)
	StopContainer(name string) (string, error)
}

type CommandBuilder interface {
	ExecCommand(args ...string) *exec.Cmd
	RemoteCopyCommand(remoteSrc string, localDst string) *exec.Cmd
}

func New() (Executor, error) {
	if config.HostInfo().Kind == "k8s" {
		return newK8sExecutor()
	}
	return newDockerExecutor()
}
