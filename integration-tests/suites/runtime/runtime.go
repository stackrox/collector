package runtime

import (
	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/config"
)

type Container struct {
	Name    string
	Id      string
	ShortId string
}

type ContainerOptions struct {
	Background bool
	Network    string
	Privileged bool
	Volumes    []string
	Env        map[string]string
}

type Runtime interface {
	StartContainer(name string, options ContainerOptions) (*Container, error)
	StopContainer(container *Container, remove bool) error
	PullImage(image string) error
	IsRunning(container *Container) (bool, error)
	Exec(container *Container, stdin bool, command ...string) (string, error)
	OnExit() error
}

func NewRuntime(options config.Runtime, exec common.Executor) Runtime {
	switch options.Command {
	case "docker":
		return NewDockerRuntime(exec)
	case "podman":
		return NewPodmanRuntime(exec)
	default:
		panic("unknown container runtime: " + options.Command)
	}
}
