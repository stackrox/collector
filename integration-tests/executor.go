package integrationtests

import (
	"fmt"
	"os"
	"os/exec"
	"strconv"
	"strings"

	"github.com/pkg/errors"
)

var (
	debug = false
)

type Executor interface {
	CopyFromHost(src string, dst string) (string, error)
	PullImage(image string) error
	Exec(args ...string) (string, error)
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
	if gcb.user == "" && strings.HasPrefix(gcb.vmType, "coreos") {
		gcb.user = "core"
	}
	return gcb
}

func NewLocalCommandBuilder() CommandBuilder {
	return &localCommandBuilder{}
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

func (e *executor) Exec(args ...string) (string, error) {
	return e.RunCommand(e.builder.ExecCommand(args...))
}

func (e *executor) ExecRetry(args ...string) (res string, err error) {
	maxAttempts := 3
	attempt := 0
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

func (e *executor) CopyFromHost(src string, dst string) (res string, err error) {
	cmd := e.builder.RemoteCopyCommand(src, dst)
	maxAttempts := 3
	attempt := 0
	for attempt < maxAttempts {
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
	_, err := e.Exec("docker", "image", "inspect", image)
	if err == nil {
		return nil
	}
	_, err = e.ExecRetry("docker", "pull", image)
	return err
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
	for _, arg := range args {
		argQuoted := strconv.Quote(arg)
		argQuotedTrimmed := strings.Trim(argQuoted, "\"")
		if arg != argQuotedTrimmed {
			arg = argQuoted
		}
		cmdArgs = append(cmdArgs, arg)
	}
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
	for _, arg := range args {
		argQuoted := strconv.Quote(arg)
		argQuotedTrimmed := strings.Trim(argQuoted, "\"")
		if arg != argQuotedTrimmed {
			arg = argQuoted
		}
		cmdArgs = append(cmdArgs, arg)
	}
	return exec.Command("ssh", cmdArgs...)
}

func (e *sshCommandBuilder) RemoteCopyCommand(remoteSrc string, localDst string) *exec.Cmd {
	args := []string{
		"-o", "StrictHostKeyChecking=no", "-i", e.keyPath,
		e.user + "@" + e.address + ":" + remoteSrc, localDst}
	return exec.Command("scp", args...)
}
