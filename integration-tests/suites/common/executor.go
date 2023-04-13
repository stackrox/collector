package common

import (
	"fmt"
	"io"
	"os"
	"os/exec"
	"strconv"
	"strings"

	"github.com/pkg/errors"
)

var (
	debug = false

	RuntimeCommand = ReadEnvVarWithDefault("RUNTIME_COMMAND", "docker")
	RuntimeSocket  = ReadEnvVarWithDefault("RUNTIME_SOCKET", "/var/run/docker.sock")
	RuntimeAsRoot  = ReadBoolEnvVar("RUNTIME_AS_ROOT")
)

type Executor interface {
	CopyFromHost(src string, dst string) (string, error)
	PullImage(image string) error
	IsContainerRunning(image string) (bool, error)
	ExitCode(container string) (int, error)
	Exec(args ...string) (string, error)
	ExecWithStdin(pipedContent string, args ...string) (string, error)
	ExecRetry(args ...string) (string, error)
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

func ReadEnvVar(env string) string {
	if e, ok := os.LookupEnv(env); ok {
		return e
	}
	return ""
}

func ReadEnvVarWithDefault(env string, def string) string {
	if e, ok := os.LookupEnv(env); ok {
		return e
	}
	return def
}

func ReadBoolEnvVar(env string) bool {
	e, err := strconv.ParseBool(ReadEnvVarWithDefault(env, "false"))
	if err != nil {
		return false
	}
	return e
}

func NewSSHCommandBuilder() CommandBuilder {
	return &sshCommandBuilder{
		user:    ReadEnvVar("SSH_USER"),
		address: ReadEnvVar("SSH_ADDRESS"),
		keyPath: ReadEnvVar("SSH_KEY_PATH"),
	}
}

func NewGcloudCommandBuilder() CommandBuilder {
	gcb := &gcloudCommandBuilder{
		user:     ReadEnvVar("GCLOUD_USER"),
		instance: ReadEnvVar("GCLOUD_INSTANCE"),
		options:  ReadEnvVar("GCLOUD_OPTIONS"),
		vmType:   ReadEnvVarWithDefault("VM_CONFIG", "default"),
	}
	if gcb.user == "" && (strings.Contains(gcb.vmType, "coreos") || strings.Contains(gcb.vmType, "flatcar")) {
		gcb.user = "core"
	}
	return gcb
}

func NewLocalCommandBuilder() CommandBuilder {
	return &localCommandBuilder{}
}

// SELinux needs to be set to permissive in order for the mock GRPC to work in fedora coreos
func setSelinuxPermissiveIfNeeded() error {
	if isSelinuxPermissiveNeeded() {
		return setSelinuxPermissive()
	}
	return nil
}

func isSelinuxPermissiveNeeded() bool {
	vmType := ReadEnvVarWithDefault("VM_CONFIG", "default")
	if strings.Contains(vmType, "coreos") || strings.Contains(vmType, "rhcos") {
		return true
	}
	if strings.Contains(vmType, "rhel-7") {
		collectionMethod := ReadEnvVarWithDefault("COLLECTION_METHOD", "kernel_module")
		if collectionMethod == "ebpf" {
			return true
		}
	}
	return false
}

func setSelinuxPermissive() error {
	cmd := []string{"sudo", "setenforce", "0"}
	e := NewExecutor()
	_, err := e.Exec(cmd...)
	if err != nil {
		fmt.Printf("Error: Unable to set SELinux to permissive. %v\n", err)
	}
	return err
}

func NewExecutor() Executor {
	e := executor{}
	switch ReadEnvVarWithDefault("REMOTE_HOST_TYPE", "local") {
	case "ssh":
		e.builder = NewSSHCommandBuilder()
	case "gcloud":
		e.builder = NewGcloudCommandBuilder()
	case "local":
		e.builder = NewLocalCommandBuilder()
	}
	return &e
}

// Execute provided command with retries on error.
func (e *executor) Exec(args ...string) (string, error) {
	if args[0] == RuntimeCommand && RuntimeAsRoot {
		args = append([]string{"sudo"}, args...)
	}
	return Retry(func() (string, error) {
		return e.RunCommand(e.builder.ExecCommand(args...))
	})
}

func (e *executor) ExecRetry(args ...string) (res string, err error) {
	maxAttempts := 3
	attempt := 0

	if args[0] == RuntimeCommand && RuntimeAsRoot {
		args = append([]string{"sudo"}, args...)
	}

	for attempt < maxAttempts {
		if attempt > 0 {
			fmt.Printf("Retrying (%v) (%d of %d) Error: %v\n", args, attempt, maxAttempts, err)
		}
		attempt++
		res, err = e.RunCommand(e.builder.ExecCommand(args...))
		if err == nil {
			break
		}
	}
	return res, err
}

func (e *executor) RunCommand(cmd *exec.Cmd) (string, error) {
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

func (e *executor) ExecWithStdin(pipedContent string, args ...string) (res string, err error) {
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

func (e *executor) PullImage(image string) error {
	_, err := e.Exec(RuntimeCommand, "image", "inspect", image)
	if err == nil {
		return nil
	}
	_, err = e.ExecRetry(RuntimeCommand, "pull", image)
	return err
}

func (e *executor) IsContainerRunning(containerID string) (bool, error) {
	result, err := e.Exec(RuntimeCommand, "inspect", containerID, "--format='{{.State.Running}}'")
	if err != nil {
		return false, err
	}
	return strconv.ParseBool(strings.Trim(result, "\"'"))
}

func (e *executor) ExitCode(containerID string) (int, error) {
	result, err := e.Exec(RuntimeCommand, "inspect", containerID, "--format='{{.State.ExitCode}}'")
	if err != nil {
		return -1, err
	}
	return strconv.Atoi(strings.Trim(result, "\"'"))
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
