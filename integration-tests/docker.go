package integrationtests

import (
	"fmt"
	"strconv"
	"strings"
	"time"
)

const (
	containerNamePrefix = "collector-int-"
)

// Docker is a wrapper around the Executor to provide
// docker-specific commands, centralizing all logic related
// to running docker.
type Docker struct {
	executor Executor
}

// NewDocker creates a new Docker object, using the provided Executor
func NewDocker(executor Executor) Docker {
	return Docker{
		executor,
	}
}

// Logs gets the log for a given container. It is equivalent to running
// docker logs <name>
func (d *Docker) Logs(name string) (string, error) {
	logs, err := d.runDocker("logs", d.containerName(name))
	if err != nil {
		return "", err
	}

	return logs, nil
}

// Run starts a new container, with the provided name, and using the given args.
func (d *Docker) Run(name string, args ...string) (string, error) {
	cmd := []string{
		"--name", d.containerName(name),
	}

	cmd = append(cmd, args...)
	return d.runDocker("run", cmd...)
}

// Exec runs a command within a given named container.
func (d *Docker) Exec(name string, cmd ...string) (string, error) {
	args := append([]string{d.containerName(name)}, cmd...)
	return d.runDocker("exec", args...)
}

// PS is equivalent to docker ps <args>
func (d *Docker) PS(args ...string) (string, error) {
	return d.runDocker("ps", args...)
}

// Inspect is equivalent to docker inspect <args>
func (d *Docker) Inspect(name string, args ...string) (string, error) {
	args = append(args, d.containerName(name))
	return d.runDocker("inspect", args...)
}

// Stop attempts to stop the named containers, up to the given timeout.
// If the timeout passes, the containers will be forcibly killed.
func (d *Docker) Stop(timeout time.Duration, names ...string) error {
	args := append([]string{"-t", fmt.Sprintf("%d", timeout)}, d.containerNames(names)...)
	_, err := d.runDocker("stop", args...)
	return err
}

// Remove will remove the named containers.
// It is equivalent to docker rm -fv <names>
func (d *Docker) Remove(names ...string) error {
	args := append([]string{"-fv"}, d.containerNames(names)...)
	_, err := d.runDocker("rm", args...)
	return err
}

// Kill attempts to kill the named containers.
// It is equivalent to docker kill <names>
func (d *Docker) Kill(names ...string) error {
	_, err := d.runDocker("kill", d.containerNames(names)...)
	return err
}

// ImageExists checks for the existence of a given image within the docker
// context
func (d *Docker) ImageExists(image string) bool {
	_, err := d.runDocker("image", "inspect", image)
	return err == nil
}

// Pull attempts to pull the given image, repeating as necessary until an error
// or until the image has been successfully pulled.
func (d *Docker) Pull(image string) error {
	_, err := d.runDockerRetry("pull", image)
	return err
}

// ExitCode gets the exit code for the named container.
func (d *Docker) ExitCode(name string) (int, error) {
	result, err := d.Inspect(name, "--format='{{.State.ExitCode}}'")
	if err != nil {
		return -1, err
	}
	return strconv.Atoi(strings.Trim(result, "\"'"))
}

// IsContainerRunning checks if the named container is currently running.
func (d *Docker) IsContainerRunning(name string) (bool, error) {
	result, err := d.Inspect(name, "--format='{{.State.Running}}'")
	if err != nil {
		return false, err
	}
	return strconv.ParseBool(strings.Trim(result, "\"'"))
}

// runDocker runs the docker client, with the provided command and args.
// It uses the executor, which may run locally, over SSH, or using gcloud
func (d *Docker) runDocker(cmd string, args ...string) (string, error) {
	fullCmd := append([]string{"docker", cmd}, args...)
	return d.executor.Exec(fullCmd...)
}

// runDocker runs the docker client, with the provided command and args, a number
// of times until failure, or success.
// It uses the executor, which may run locally, over SSH, or using gcloud
func (d *Docker) runDockerRetry(cmd string, args ...string) (string, error) {
	fullCmd := append([]string{"docker", cmd}, args...)
	return d.executor.ExecRetry(fullCmd...)
}

// containerName appends a prefix to the container name to better distinguish
// containers associated with the integration tests.
func (d *Docker) containerName(name string) string {
	return fmt.Sprintf("%s%s", containerNamePrefix, name)
}

// containerNames is a helper function that prefixes a list of names
// for the same reasons as containerName() above.
func (d *Docker) containerNames(names []string) []string {
	processed := make([]string, len(names))
	for i, name := range names {
		processed[i] = d.containerName(name)
	}
	return processed
}
