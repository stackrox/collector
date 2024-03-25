package common

import (
	"io"
	"os/exec"
	"strconv"
	"strings"

	"github.com/pkg/errors"

	"github.com/stackrox/collector/integration-tests/suites/config"
	"github.com/stackrox/collector/integration-tests/suites/log"
)

var (
	RuntimeCommand = config.RuntimeInfo().Command
	RuntimeSocket  = config.RuntimeInfo().Socket
	RuntimeAsRoot  = config.RuntimeInfo().RunAsRoot
)

type Executor interface {
	CopyFromHost(src string, dst string) (string, error)
	PullImage(image string) error
	IsContainerRunning(image string) (bool, error)
	ContainerExists(container string) (bool, error)
	ExitCode(container string) (int, error)
	Exec(args ...string) (string, error)
	ExecWithErrorCheck(errCheckFn func(string, error) error, args ...string) (string, error)
	ExecWithStdin(pipedContent string, args ...string) (string, error)
	ExecWithoutRetry(args ...string) (string, error)
	KillContainer(name string) (string, error)
	RemoveContainer(name string) (string, error)
	StopContainer(name string) (string, error)
}

type CommandBuilder interface {
	ExecCommand(args ...string) *exec.Cmd
	RemoteCopyCommand(remoteSrc string, localDst string) *exec.Cmd
}

type executor struct {
	builder CommandBuilder
}

type sshCommandBuilder struct {
	user    string
	address string
	keyPath string
}

type gcloudCommandBuilder struct {
	user     string
	instance string
	options  string
	vmType   string
}

type localCommandBuilder struct {
}

func NewSSHCommandBuilder() CommandBuilder {
	host_info := config.HostInfo()
	return &sshCommandBuilder{
		user:    host_info.User,
		address: host_info.Address,
		keyPath: host_info.Options,
	}
}

func NewGcloudCommandBuilder() CommandBuilder {
	host_info := config.HostInfo()
	gcb := &gcloudCommandBuilder{
		user:     host_info.User,
		instance: host_info.Address,
		options:  host_info.Options,
		vmType:   config.VMInfo().Config,
	}
	if gcb.user == "" && (strings.Contains(gcb.vmType, "coreos") || strings.Contains(gcb.vmType, "flatcar")) {
		gcb.user = "core"
	}
	return gcb
}

func NewLocalCommandBuilder() CommandBuilder {
	return &localCommandBuilder{}
}

func NewExecutor() Executor {
	e := executor{}
	switch config.HostInfo().Kind {
	case "ssh":
		e.builder = NewSSHCommandBuilder()
	case "gcloud":
		e.builder = NewGcloudCommandBuilder()
	case "local":
		e.builder = NewLocalCommandBuilder()
	}
	return &e
}

// Exec executes the provided command with retries on non-zero error from the command.
func (e *executor) Exec(args ...string) (string, error) {
	if args[0] == RuntimeCommand && RuntimeAsRoot {
		args = append([]string{"sudo"}, args...)
	}
	return Retry(func() (string, error) {
		return e.RunCommand(e.builder.ExecCommand(args...))
	})
}

// ExecWithErrorCheck executes the provided command, retrying if an error occurs
// and the command's output does not contain any of the accepted output contents.
func (e *executor) ExecWithErrorCheck(errCheckFn func(string, error) error, args ...string) (string, error) {
	if args[0] == RuntimeCommand && RuntimeAsRoot {
		args = append([]string{"sudo"}, args...)
	}
	return RetryWithErrorCheck(errCheckFn, func() (string, error) {
		return e.RunCommand(e.builder.ExecCommand(args...))
	})
}

// ExecWithoutRetry executes provided command once, without retries.
func (e *executor) ExecWithoutRetry(args ...string) (string, error) {
	if args[0] == RuntimeCommand && RuntimeAsRoot {
		args = append([]string{"sudo"}, args...)
	}
	return e.RunCommand(e.builder.ExecCommand(args...))
}

func (e *executor) RunCommand(cmd *exec.Cmd) (string, error) {
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

func (e *executor) ExecWithStdin(pipedContent string, args ...string) (res string, err error) {

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

func (e *executor) CopyFromHost(src string, dst string) (res string, err error) {
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

func (e *executor) PullImage(image string) error {
	_, err := e.Exec(RuntimeCommand, "image", "inspect", image)
	if err == nil {
		return nil
	}
	_, err = e.Exec(RuntimeCommand, "pull", image)
	return err
}

func (e *executor) IsContainerRunning(containerID string) (bool, error) {
	result, err := e.ExecWithoutRetry(RuntimeCommand, "inspect", containerID, "--format='{{.State.Running}}'")
	if err != nil {
		return false, err
	}
	return strconv.ParseBool(strings.Trim(result, "\"'"))
}

func (e *executor) ContainerExists(container string) (bool, error) {
	_, err := e.ExecWithoutRetry(RuntimeCommand, "inspect", container)
	if err != nil {
		return false, err
	}
	return true, nil
}

func (e *executor) ExitCode(containerID string) (int, error) {
	result, err := e.Exec(RuntimeCommand, "inspect", containerID, "--format='{{.State.ExitCode}}'")
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
func (e *executor) KillContainer(name string) (string, error) {
	return e.ExecWithErrorCheck(containerErrorCheckFunction(name, "kill"), RuntimeCommand, "kill", name)
}

// RemoveContainer runs the remove operation on the provided container name
func (e *executor) RemoveContainer(name string) (string, error) {
	return e.ExecWithErrorCheck(containerErrorCheckFunction(name, "remove"), RuntimeCommand, "rm", name)
}

// StopContainer runs the stop operation on the provided container name
func (e *executor) StopContainer(name string) (string, error) {
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

func (e *gcloudCommandBuilder) ExecCommand(args ...string) *exec.Cmd {
	cmdArgs := []string{"compute", "ssh"}
	if len(e.options) > 0 {
		opts := strings.Split(e.options, " ")
		cmdArgs = append(cmdArgs, opts...)
	}
	userInstance := e.instance
	if e.user != "" {
		userInstance = e.user + "@" + e.instance
	}

	cmdArgs = append(cmdArgs, userInstance, "--", "-T")
	cmdArgs = append(cmdArgs, QuoteArgs(args)...)
	return exec.Command("gcloud", cmdArgs...)
}

func (e *gcloudCommandBuilder) RemoteCopyCommand(remoteSrc string, localDst string) *exec.Cmd {
	cmdArgs := []string{"compute", "scp"}
	if len(e.options) > 0 {
		opts := strings.Split(e.options, " ")
		cmdArgs = append(cmdArgs, opts...)
	}
	userInstance := e.instance
	if e.user != "" {
		userInstance = e.user + "@" + e.instance
	}
	cmdArgs = append(cmdArgs, userInstance+":"+remoteSrc, localDst)
	return exec.Command("gcloud", cmdArgs...)
}

func (e *sshCommandBuilder) ExecCommand(args ...string) *exec.Cmd {
	cmdArgs := []string{
		"-o", "StrictHostKeyChecking=no", "-i", e.keyPath,
		e.user + "@" + e.address}

	cmdArgs = append(cmdArgs, QuoteArgs(args)...)
	return exec.Command("ssh", cmdArgs...)
}

func (e *sshCommandBuilder) RemoteCopyCommand(remoteSrc string, localDst string) *exec.Cmd {
	args := []string{
		"-o", "StrictHostKeyChecking=no", "-i", e.keyPath,
		e.user + "@" + e.address + ":" + remoteSrc, localDst}
	return exec.Command("scp", args...)
}
