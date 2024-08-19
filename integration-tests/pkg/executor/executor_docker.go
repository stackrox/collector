package executor

import (
	"io"
	"os/exec"
	"strconv"
	"strings"

	"github.com/pkg/errors"
	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/log"
)

var (
	RuntimeCommand = config.RuntimeInfo().Command
	RuntimeSocket  = config.RuntimeInfo().Socket
	RuntimeAsRoot  = config.RuntimeInfo().RunAsRoot
)

type dockerExecutor struct {
	builder CommandBuilder
}

type localCommandBuilder struct {
}

func newLocalCommandBuilder() CommandBuilder {
	return &localCommandBuilder{}
}

func newDockerExecutor() (*dockerExecutor, error) {
	// While this function can't fail, to conform to the
	// same construction API as other executors, we keep the
	// error return value.
	return &dockerExecutor{
		builder: newLocalCommandBuilder(),
	}, nil
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
	log.Info("%s\n", commandLine)
	stdoutStderr, err := cmd.CombinedOutput()
	trimmed := strings.Trim(string(stdoutStderr), "\"\n")
	log.Debug("Run Output: %s\n", trimmed)
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
			log.Error("Retrying (%v) (%d of %d) Error: %v\n", cmd, attempt, maxAttempts, err)
		}
		attempt++
		res, err = e.RunCommand(cmd)
		if err == nil {
			break
		}
	}
	return res, err
}

func (e *dockerExecutor) PullImage(image string) error {
	_, err := e.Exec(RuntimeCommand, "image", "inspect", image)
	if err == nil {
		return nil
	}
	_, err = e.Exec(RuntimeCommand, "pull", image)
	return err
}

func (e *dockerExecutor) IsContainerRunning(containerID string) (bool, error) {
	result, err := e.ExecWithoutRetry(RuntimeCommand, "inspect", containerID, "--format='{{.State.Running}}'")
	if err != nil {
		return false, err
	}
	return strconv.ParseBool(strings.Trim(result, "\"'"))
}

func (e *dockerExecutor) ContainerID(cf ContainerFilter) string {
	result, err := e.ExecWithoutRetry(RuntimeCommand, "ps", "-aqf", "name=^"+cf.Name+"$")
	if err != nil {
		return ""
	}

	return strings.Trim(result, "\"")
}

func (e *dockerExecutor) ContainerExists(cf ContainerFilter) (bool, error) {
	_, err := e.ExecWithoutRetry(RuntimeCommand, "inspect", cf.Name)
	if err != nil {
		return false, err
	}
	return true, nil
}

func (e *dockerExecutor) ExitCode(cf ContainerFilter) (int, error) {
	result, err := e.Exec(RuntimeCommand, "inspect", cf.Name, "--format='{{.State.ExitCode}}'")
	if err != nil {
		return -1, err
	}
	return strconv.Atoi(strings.Trim(result, "\"'"))
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
func (e *dockerExecutor) KillContainer(name string) (string, error) {
	return e.ExecWithErrorCheck(containerErrorCheckFunction(name, "kill"), RuntimeCommand, "kill", name)
}

// RemoveContainer runs the remove operation on the provided container name
func (e *dockerExecutor) RemoveContainer(cf ContainerFilter) (string, error) {
	return e.ExecWithErrorCheck(containerErrorCheckFunction(cf.Name, "remove"), RuntimeCommand, "rm", cf.Name)
}

// StopContainer runs the stop operation on the provided container name
func (e *dockerExecutor) StopContainer(name string) (string, error) {
	return e.ExecWithErrorCheck(containerErrorCheckFunction(name, "stop"), RuntimeCommand, "stop", name)
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
