package executor

import (
	"context"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"os/exec"
	"strings"

	"github.com/pkg/errors"
	"github.com/stackrox/collector/integration-tests/pkg/config"

	"github.com/docker/docker/api/types/container"
	"github.com/docker/docker/api/types/filters"
	"github.com/docker/docker/api/types/image"
	"github.com/docker/docker/api/types/registry"
	"github.com/docker/docker/client"
)

var (
	debug = false

	RuntimeCommand = config.RuntimeInfo().Command
	RuntimeSocket  = config.RuntimeInfo().Socket
	RuntimeAsRoot  = config.RuntimeInfo().RunAsRoot

	authFiles = []string{
		"$HOME/.config/containers/auth.json",
		"$HOME/.docker/config.json",
	}
)

const MAIN_REGISTRY = "quay.io"

type dockerExecutor struct {
	builder    CommandBuilder
	cli        *client.Client
	authConfig string
}

type localCommandBuilder struct {
}

func newLocalCommandBuilder() CommandBuilder {
	return &localCommandBuilder{}
}

func newDockerExecutor() (*dockerExecutor, error) {
	cli, err := client.NewClientWithOpts(client.FromEnv, client.WithAPIVersionNegotiation())
	if err != nil {
		return nil, err
	}

	auth, err := findAuth()
	if err != nil {
		return nil, err
	}

	_, err = cli.RegistryLogin(context.TODO(), *auth)
	if err != nil {
		return nil, err
	}

	b64auth, err := registry.EncodeAuthConfig(*auth)
	if err != nil {
		return nil, err
	}

	return &dockerExecutor{
		builder:    newLocalCommandBuilder(),
		cli:        cli,
		authConfig: b64auth,
	}, nil
}

func findAuth() (*registry.AuthConfig, error) {
	for _, path := range authFiles {
		expanded := os.ExpandEnv(path)

		file, err := os.Open(expanded)
		if err != nil {
			continue
		}
		defer file.Close()

		// if we find a file, then we should use it
		// so propagage errors up the stack after this point

		bytes, err := ioutil.ReadAll(file)
		if err != nil {
			return nil, err
		}

		var auths struct {
			Auths map[string]registry.AuthConfig
		}

		err = json.Unmarshal(bytes, &auths)
		if err != nil {
			return nil, err
		}

		main_auth := auths.Auths[MAIN_REGISTRY]
		main_auth.ServerAddress = MAIN_REGISTRY

		if main_auth.Username == "" && main_auth.Auth != "" {
			auth_plain, err := base64.StdEncoding.DecodeString(main_auth.Auth)
			if err != nil {
				return nil, err
			}
			split := strings.Split(string(auth_plain), ":")
			main_auth.Username = split[0]
			main_auth.Password = split[1]
		}

		return &main_auth, nil
	}

	return nil, fmt.Errorf("Unable to find any auth json files")
}

// Exec executes the provided command with retries on non-zero error from the command.
func (e *dockerExecutor) Exec(args ...string) (string, error) {
	if args[0] == RuntimeCommand && RuntimeAsRoot {
		args = append([]string{"sudo"}, args...)
	}
	return Retry(func() (string, error) {
		return e.RunCommand(e.builder.ExecCommand(args...))
	})
}

// ExecWithErrorCheck executes the provided command, retrying if an error occurs
// and the command's output does not contain any of the accepted output contents.
func (e *dockerExecutor) ExecWithErrorCheck(errCheckFn func(string, error) error, args ...string) (string, error) {
	if args[0] == RuntimeCommand && RuntimeAsRoot {
		args = append([]string{"sudo"}, args...)
	}
	return RetryWithErrorCheck(errCheckFn, func() (string, error) {
		return e.RunCommand(e.builder.ExecCommand(args...))
	})
}

// ExecWithoutRetry executes provided command once, without retries.
func (e *dockerExecutor) ExecWithoutRetry(args ...string) (string, error) {
	if args[0] == RuntimeCommand && RuntimeAsRoot {
		args = append([]string{"sudo"}, args...)
	}
	return e.RunCommand(e.builder.ExecCommand(args...))
}

func (e *dockerExecutor) RunCommand(cmd *exec.Cmd) (string, error) {
	if cmd == nil {
		return "", nil
	}
	commandLine := strings.Join(cmd.Args, " ")
	if debug {
		fmt.Printf("Run: %s\n", commandLine)
	}
	stdoutStderr, err := cmd.CombinedOutput()
	trimmed := strings.Trim(string(stdoutStderr), "\"\n")
	if debug {
		fmt.Printf("Run Output: %s\n", trimmed)
	}
	if err != nil {
		err = errors.Wrapf(err, "Command Failed: %s\nOutput: %s\n", commandLine, trimmed)
	}
	return trimmed, err
}

func (e *dockerExecutor) ExecWithStdin(pipedContent string, args ...string) (res string, err error) {

	if args[0] == RuntimeCommand && RuntimeAsRoot {
		args = append([]string{"sudo"}, args...)
	}

	cmd := e.builder.ExecCommand(args...)

	stdin, err := cmd.StdinPipe()
	if err != nil {
		return "", err
	}

	go func() {
		defer stdin.Close()
		io.WriteString(stdin, pipedContent)
	}()

	return e.RunCommand(cmd)
}

func (e *dockerExecutor) CopyFromHost(src string, dst string) (res string, err error) {
	maxAttempts := 3
	attempt := 0
	for attempt < maxAttempts {
		cmd := e.builder.RemoteCopyCommand(src, dst)
		if attempt > 0 {
			fmt.Printf("Retrying (%v) (%d of %d) Error: %v\n", cmd, attempt, maxAttempts, err)
		}
		attempt++
		res, err = e.RunCommand(cmd)
		if err == nil {
			break
		}
	}
	return res, err
}

func (e *dockerExecutor) PullImage(ref string) error {
	imgFilter := filters.NewArgs(filters.KeyValuePair{
		Key:   "reference",
		Value: ref,
	})

	images, err := e.cli.ImageList(context.TODO(), image.ListOptions{
		Filters: imgFilter,
	})

	if err != nil {
		return err
	}

	if len(images) != 0 {
		// image already exists; don't pull
		// TODO: might want to enable force pull
		return nil
	}
	reader, err := e.cli.ImagePull(context.TODO(), ref, image.PullOptions{
		RegistryAuth: e.authConfig,
	})
	if err != nil {
		return err
	}
	defer reader.Close()

	io.Copy(ioutil.Discard, reader)
	return nil
}

func (e *dockerExecutor) IsContainerRunning(containerID string) (bool, error) {
	containerJSON, err := e.cli.ContainerInspect(context.TODO(), containerID)
	if err != nil {
		return false, err
	}

	return containerJSON.State.Running, nil
}

func (e *dockerExecutor) ContainerID(cf ContainerFilter) string {
	filter := filters.NewArgs(filters.KeyValuePair{
		Key:   "name",
		Value: cf.Name,
	})

	lops := container.ListOptions{
		Filters: filter,
	}

	containers, err := e.cli.ContainerList(context.TODO(), lops)
	if err != nil || len(containers) != 1 {
		return ""
	}

	return containers[0].ID
}

func (e *dockerExecutor) ContainerExists(cf ContainerFilter) (bool, error) {
	_, err := e.cli.ContainerInspect(context.TODO(), cf.Name)
	if err != nil {
		return false, err
	}

	return true, nil
}

func (e *dockerExecutor) ExitCode(cf ContainerFilter) (int, error) {
	containerJSON, err := e.cli.ContainerInspect(context.TODO(), cf.Name)
	if err != nil {
		return -1, err
	}

	return containerJSON.State.ExitCode, nil
}

// checkContainerCommandError returns nil if the output of the container
// command indicates retries are not needed.
func checkContainerCommandError(name string, cmd string, output string, err error) error {
	for _, str := range []string{
		"no such container",
		"cannot " + cmd + " container",
		"can only " + cmd + " running containers",
	} {
		if strings.Contains(strings.ToLower(output), strings.ToLower(str)) {
			return nil
		}
	}
	return err
}

func containerErrorCheckFunction(name string, cmd string) func(string, error) error {
	return func(stdout string, err error) error {
		return checkContainerCommandError(name, cmd, stdout, err)
	}
}

// KillContainer runs the kill operation on the provided container name
func (e *dockerExecutor) KillContainer(name string) error {
	return e.cli.ContainerKill(context.TODO(), name, "KILL")
}

// RemoveContainer runs the remove operation on the provided container name
func (e *dockerExecutor) RemoveContainer(cf ContainerFilter) error {
	return e.cli.ContainerRemove(context.TODO(), cf.Name, container.RemoveOptions{})
}

// StopContainer runs the stop operation on the provided container name
func (e *dockerExecutor) StopContainer(name string) error {
	return e.cli.ContainerStop(context.TODO(), name, container.StopOptions{})
}

func (e *localCommandBuilder) ExecCommand(execArgs ...string) *exec.Cmd {
	return exec.Command(execArgs[0], execArgs[1:]...)
}

func (e *localCommandBuilder) RemoteCopyCommand(remoteSrc string, localDst string) *exec.Cmd {
	if remoteSrc != localDst {
		return exec.Command("cp", remoteSrc, localDst)
	}
	return nil
}
