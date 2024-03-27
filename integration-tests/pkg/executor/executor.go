package executor

import (
	"os/exec"

	"github.com/stackrox/collector/integration-tests/pkg/config"
)

type Executor interface {
	CopyFromHost(src string, dst string) (string, error)
	PullImage(image string) error
	IsContainerRunning(container string) (bool, error)
	ContainerExists(container interface{}) (bool, error)
	ContainerID(container interface{}) string
	ExitCode(container interface{}) (int, error)
	Exec(args ...string) (string, error)
	ExecWithErrorCheck(errCheckFn func(string, error) error, args ...string) (string, error)
	ExecWithStdin(pipedContent string, args ...string) (string, error)
	ExecWithoutRetry(args ...string) (string, error)
	KillContainer(name string) (string, error)
	RemoveContainer(name interface{}) (string, error)
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
