package runtime

import (
	"errors"
	"strconv"
	"strings"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/config"
)

type DockerRuntime struct {
	command    string
	executor   common.Executor
	containers map[string]*Container
}

func (d *DockerRuntime) StartContainer(name string, options ContainerOptions) (*Container, error) {
	if _, exists := d.containers[name]; exists {
		return nil, errors.New("Container already registered called " + name)
	}

	cmd := []string{d.command, "--name", name}

	if options.Background {
		cmd = append(cmd, "-d")
	}

	for _, v := range options.Volumes {
		cmd = append(cmd, "-v", v)
	}

	for key, value := range options.Env {
		cmd = append(cmd, "-e", key+"="+value)
	}

	if options.Network != "" {
		cmd = append(cmd, "--network="+options.Network)
	}

	output, err := common.Retry(func() (string, error) {
		return d.executor.Exec(cmd...)
	})

	if err != nil {
		return nil, err
	}

	id := strings.Split(output, "\n")[0]
	container := &Container{
		Name:    name,
		Id:      id,
		ShortId: common.ContainerShortID(id),
	}

	d.containers[name] = container
	return container, nil
}

func (d *DockerRuntime) StopContainer(container *Container, remove bool) error {
	_, err := d.executor.Exec(d.command, "stop", "-t", config.StopTimeout(), container.Id)
	if err != nil {
		return err
	}

	if remove {
		_, err := d.executor.Exec(d.command, "rm", "-f", container.Id)
		if err != nil {
			return err
		}
		delete(d.containers, container.Name)
	}

	return nil
}

func (d *DockerRuntime) PullImage(image string) error {
	_, err := d.executor.Exec(d.command, "image", "inspect", image)
	if err == nil {
		return nil
	}
	_, err = d.executor.ExecRetry(d.command, "pull", image)
	return err
}

func (d *DockerRuntime) IsRunning(container *Container) (bool, error) {
	result, err := d.executor.Exec(d.command, "inspect", container.Id, "--format='{{.State.Running}}'")
	if err != nil {
		return false, err
	}
	return strconv.ParseBool(strings.Trim(result, "\"'"))
}

func (d *DockerRuntime) OnExit() error {
	return nil
}

func NewDockerRuntime(executor common.Executor) Runtime {
	return &DockerRuntime{
		command:    "docker",
		executor:   executor,
		containers: make(map[string]*Container),
	}
}
