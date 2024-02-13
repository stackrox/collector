package common

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"os/exec"
	"strconv"
	"strings"
	"time"

	"github.com/pkg/errors"

	"github.com/docker/docker/api/types"
	containerTypes "github.com/docker/docker/api/types/container"
	"github.com/docker/docker/api/types/filters"
	"github.com/docker/docker/pkg/stdcopy"

	"github.com/docker/docker/client"

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
	ExecContainer(containerName string, command []string) (string, error)
	GetContainerPort(containerID string) (string, error)
	GetContainerIP(containerID string) (string, error)
	GetContainerLogs(containerID string) (string, error)
	StartContainer(config ContainerStartConfig) (string, error)
	GetContainerHealthCheck(containerID string) (string, error)
	IsContainerFoundFiltered(containerID, filter string) (bool, error)
}

type CommandBuilder interface {
	ExecCommand(args ...string) *exec.Cmd
	RemoteCopyCommand(remoteSrc string, localDst string) *exec.Cmd
}

type executor struct {
	builder       CommandBuilder
	containerExec ContainerRuntimeExecutor
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

type ContainerRuntimeExecutor interface {
	ExecContainer(containerName string, command []string) (string, error)
	KillContainer(containerID string) error
	StopContainer(containerID string) error
	RemoveContainer(containerID string) error
	//PullImage(image string) error
	GetContainerLogs(containerID string) (string, error)
	CheckImageExists(image string) (bool, error)
	GetContainerPort(containerID string) (string, error)
	GetContainerIP(containerID string) (string, error)
	CheckContainerRunning(containerID string) (bool, error)
	CheckContainerExists(containerID string) (bool, error)
	GetContainerHealthCheck(containerID string) (string, error)
	GetContainerExitCode(containerID string) (int, error)
	StartContainer(config ContainerStartConfig) (string, error)
	IsContainerFoundFiltered(containerID, filter string) (bool, error)
}

type DockerExecutor struct {
	client *client.Client
}

type ContainerStartConfig struct {
	Name        string
	Image       string
	Privileged  bool
	NetworkMode string
	Mounts      map[string]string
	Env         map[string]string
	Command     []string
	EntryPoint  []string
}

func NewDockerExecutor() (*DockerExecutor, error) {
	cli, err := client.NewClientWithOpts(client.FromEnv, client.WithAPIVersionNegotiation())
	if err != nil {
		return nil, err
	}
	return &DockerExecutor{client: cli}, nil
}

// Options needed
// stats container
//  Mount docker socket -> args := []string{"-v", common.RuntimeSocket + ":/var/run/docker.sock", image}
// benchmark container
//  Env Var -> "--env", "FORCE_TIMES_TO_RUN=1",
//  Command
// collector container
//  --privileged
//  --network=host
//  --env vars
//  mounts

func convertMapToSlice(env map[string]string) []string {
	var result []string
	for k, v := range env {
		result = append(result, k+"="+v)
	}
	return result
}

func (d *DockerExecutor) IsContainerFoundFiltered(containerID, filter string) (bool, error) {
	ctx := context.Background()
	defer d.client.Close()

	parts := strings.SplitN(filter, "=", 2)
	if len(parts) != 2 {
		return false, fmt.Errorf("filter format is invalid")
	}
	filterKey, filterValue := parts[0], parts[1]

	filterArgs := filters.NewArgs()
	filterArgs.Add(filterKey, filterValue)

	if containerID != "" {
		filterArgs.Add("id", containerID)
	}

	containers, err := d.client.ContainerList(ctx, types.ContainerListOptions{
		Filters: filterArgs,
		All:     true,
	})
	if err != nil {
		return false, err
	}

	found := false
	for _, c := range containers {
		if ContainerShortID(containerID) == ContainerShortID(c.ID) {
			found = true
		}
	}
	log.Info("[dockerclient] checked %s for filter %s (%t)\n", ContainerShortID(containerID), filter, found)

	return found, nil
}

func (d *DockerExecutor) GetContainerHealthCheck(containerID string) (string, error) {
	ctx := context.Background()
	defer d.client.Close()

	container, err := d.client.ContainerInspect(ctx, containerID)
	if err != nil {
		return "", fmt.Errorf("error inspecting container: %w", err)
	}

	if container.Config.Healthcheck == nil {
		return "", fmt.Errorf("container %s does not have a health check", containerID)
	}

	log.Info("[dockerclient] %s has healthcheck: %s\n", containerID, strings.Join(container.Config.Healthcheck.Test, " "))
	return strings.Join(container.Config.Healthcheck.Test, " "), nil
}

func (d *DockerExecutor) StartContainer(startConfig ContainerStartConfig) (string, error) {
	ctx := context.Background()
	defer d.client.Close()

	binds := []string{}
	volumes := map[string]struct{}{}

	for containerPath, hostPath := range startConfig.Mounts {
		if hostPath == "" {
			volumes[containerPath] = struct{}{}
		} else {
			bind := fmt.Sprintf("%s:%s", hostPath, containerPath)
			binds = append(binds, bind)
			volumes[containerPath] = struct{}{}
		}
	}

	containerConfig := &containerTypes.Config{
		Image:      startConfig.Image,
		Env:        convertMapToSlice(startConfig.Env),
		Cmd:        startConfig.Command,
		Entrypoint: startConfig.EntryPoint,
		Volumes:    volumes,
	}

	hostConfig := &containerTypes.HostConfig{
		NetworkMode: containerTypes.NetworkMode(startConfig.NetworkMode),
		Privileged:  startConfig.Privileged,
		Binds:       binds,
	}

	resp, err := d.client.ContainerCreate(ctx, containerConfig, hostConfig, nil, nil, startConfig.Name)
	if err != nil {
		return "", err
	}

	if err := d.client.ContainerStart(ctx, resp.ID, types.ContainerStartOptions{}); err != nil {
		return "", err
	}

	log.Info("[dockerclient] container start %s with %s (%s)\n",
		startConfig.Name, startConfig.Image, ContainerShortID(resp.ID))
	return resp.ID, nil
}

func (d *DockerExecutor) ExecContainer(containerName string, command []string) (string, error) {
	ctx := context.Background()
	defer d.client.Close()
	execConfig := types.ExecConfig{
		AttachStdout: true,
		AttachStderr: true,
		Cmd:          command,
	}

	resp, err := d.client.ContainerExecCreate(ctx, containerName, execConfig)
	if err != nil {
		return "", fmt.Errorf("error creating exec: %w", err)
	}

	execStartCheck := types.ExecStartCheck{Detach: false, Tty: false}
	attachResp, err := d.client.ContainerExecAttach(ctx, resp.ID, execStartCheck)
	if err != nil {
		return "", fmt.Errorf("error attaching to exec: %w", err)
	}
	defer attachResp.Close()

	var outBuf bytes.Buffer
	_, err = io.Copy(&outBuf, attachResp.Reader)
	if err != nil {
		return "", fmt.Errorf("error reading exec output: %w", err)
	}

	execInspect, err := d.client.ContainerExecInspect(ctx, resp.ID)
	if err != nil {
		return "", fmt.Errorf("error inspecting exec: %w", err)
	}

	log.Info("[dockerclient] container exec %s %v (exitCode=%d, outBytes=%d)\n",
		containerName, command, execInspect.ExitCode, outBuf.Len())
	return outBuf.String(), nil
}

func (d *DockerExecutor) GetContainerLogs(containerID string) (string, error) {
	ctx := context.Background()
	defer d.client.Close()
	options := types.ContainerLogsOptions{ShowStdout: true, ShowStderr: true}
	logsReader, err := d.client.ContainerLogs(ctx, containerID, options)
	if err != nil {
		return "", fmt.Errorf("error getting container logs: %w", err)
	}
	defer logsReader.Close()

	// Demultiplex stderr/stdout
	var sbStdOut, sbStdErr strings.Builder
	if _, err := stdcopy.StdCopy(&sbStdOut, &sbStdErr, logsReader); err != nil {
		return "", fmt.Errorf("error copying logs: %w", err)
	}

	log.Info("[dockerclient] logs %s (%d bytes)\n", containerID, sbStdOut.Len())
	return sbStdOut.String(), nil
}

func (d *DockerExecutor) KillContainer(containerID string) error {
	ctx := context.Background()
	defer d.client.Close()

	timeoutContext, cancel := context.WithTimeout(ctx, 5*time.Second)
	defer cancel()
	err := d.client.ContainerKill(timeoutContext, containerID, "KILL")
	if err != nil {
		return fmt.Errorf("error killing container: %w", err)
	}

	log.Info("[dockerclient] kill %s\n", containerID)
	return nil
}

func (d *DockerExecutor) StopContainer(containerID string) error {
	ctx := context.Background()
	defer d.client.Close()

	stopOptions := containerTypes.StopOptions{
		Signal:  "SIGTERM",
		Timeout: nil, // 10 secs
	}
	timeoutContext, cancel := context.WithTimeout(ctx, 10*time.Second)
	defer cancel()
	err := d.client.ContainerStop(timeoutContext, containerID, stopOptions)
	if err != nil {
		return fmt.Errorf("error stopping container: %w", err)
	}
	log.Info("[dockerclient] stop %s\n", containerID)
	return nil
}

func (d *DockerExecutor) RemoveContainer(containerID string) error {
	ctx := context.Background()
	defer d.client.Close()

	removeOptions := types.ContainerRemoveOptions{
		RemoveVolumes: true,
		Force:         true,
	}
	timeoutDuration := 10 * time.Second
	timeoutContext, cancel := context.WithTimeout(ctx, timeoutDuration)
	defer cancel()
	err := d.client.ContainerRemove(timeoutContext, containerID, removeOptions)
	if err != nil {
		return fmt.Errorf("error removing container: %w", err)
	}

	log.Info("[dockerclient] remove %s\n", containerID)
	return nil
}

func (d *DockerExecutor) inspectContainer(containerID string) (types.ContainerJSON, error) {
	ctx := context.Background()
	defer d.client.Close()
	containerJSON, err := d.client.ContainerInspect(ctx, containerID)
	if err != nil {
		return types.ContainerJSON{}, fmt.Errorf("error inspecting container: %w", err)
	}
	log.Debug("[dockerclient] inspect %s\n", containerID)
	return containerJSON, nil
}

func (d *DockerExecutor) GetContainerExitCode(containerID string) (int, error) {
	container, err := d.inspectContainer(containerID)
	if err != nil {
		return -1, err
	}
	log.Info("[dockerclient] %s exitcode=%s\n", containerID, container.State.ExitCode)
	return container.State.ExitCode, nil
}

func (d *DockerExecutor) GetContainerIP(containerID string) (string, error) {
	container, err := d.inspectContainer(containerID)
	if err != nil {
		return "", err
	}
	log.Info("[dockerclient] container IP for %s is %s\n", containerID,
		container.NetworkSettings.DefaultNetworkSettings.IPAddress)
	return container.NetworkSettings.DefaultNetworkSettings.IPAddress, nil
}

func (d *DockerExecutor) GetContainerPort(containerID string) (string, error) {
	containerJSON, err := d.inspectContainer(containerID)
	if err != nil {
		return "-1", err
	}

	containerPort := "-1"
	if len(containerJSON.NetworkSettings.Ports) > 0 {
		for portStr := range containerJSON.NetworkSettings.Ports {
			portSplit := strings.Split(string(portStr), "/")
			if len(portSplit) > 0 {
				containerPort = portSplit[0]
			}
		}
	}

	log.Info("[dockerclient] container port for %s is %s\n", containerID, containerPort)
	return containerPort, nil
}

func (d *DockerExecutor) CheckContainerHealthy(containerID string) (bool, error) {
	containerJSON, err := d.inspectContainer(containerID)
	if err != nil {
		return false, err
	}
	log.Info("[dockerclient] container %s is %v\n", containerID, containerJSON.State.Health.Status)
	return containerJSON.State.Health.Status == "healthy", nil
}

func (d *DockerExecutor) CheckContainerRunning(containerID string) (bool, error) {
	containerJSON, err := d.inspectContainer(containerID)
	if err != nil {
		return false, err
	}
	log.Info("[dockerclient] %s running: %v\n", containerID, containerJSON.State.Running)
	return containerJSON.State.Running, nil
}

func (d *DockerExecutor) CheckContainerExists(containerID string) (bool, error) {
	_, err := d.inspectContainer(containerID)
	if err != nil {
		log.Info("[dockerclient] %s does not exist\n", containerID)
		return false, err
	}

	log.Info("[dockerclient] %s exists\n", containerID)
	return true, nil
}

func (d *DockerExecutor) CheckImageExists(imageName string) (bool, error) {
	ctx := context.Background()
	defer d.client.Close()

	_, _, err := d.client.ImageInspectWithRaw(ctx, imageName)
	log.Info("[dockerclient] image %s exists (%t)\n", imageName, err == nil)
	if err == nil {
		return true, nil
	} else if client.IsErrNotFound(err) {
		return false, nil
	} else {
		return false, fmt.Errorf("error inspecting image: %w", err)
	}
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
	//var err error
	//e.containerExec, err = NewDockerExecutor()
	//if err != nil {
	//	e.containerExec = nil
	//	log.Error("%v\n", err)
	//}
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
	if e.containerExec != nil {
		exists, err := e.containerExec.CheckImageExists(image)
		if exists {
			return err
		}
	} else {
		_, err := e.Exec(RuntimeCommand, "image", "inspect", image)
		if err == nil {
			return nil
		}
	}
	_, err := e.Exec(RuntimeCommand, "pull", image)
	return err
}

func (e *executor) IsContainerRunning(containerID string) (bool, error) {
	if e.containerExec != nil {
		return e.containerExec.CheckContainerRunning(containerID)
	}
	result, err := e.ExecWithoutRetry(RuntimeCommand, "inspect", containerID, "--format='{{.State.Running}}'")
	if err != nil {
		return false, err
	}
	return strconv.ParseBool(strings.Trim(result, "\"'"))
}

func (e *executor) ContainerExists(container string) (bool, error) {
	if e.containerExec != nil {
		return e.containerExec.CheckContainerExists(container)
	}
	_, err := e.ExecWithoutRetry(RuntimeCommand, "inspect", container)
	if err != nil {
		return false, err
	}
	return true, nil
}

func (e *executor) ExitCode(containerID string) (int, error) {
	if e.containerExec != nil {
		return e.containerExec.GetContainerExitCode(containerID)
	}
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

func (e *executor) StartContainer(startConfig ContainerStartConfig) (string, error) {
	if e.containerExec != nil {
		return e.containerExec.StartContainer(startConfig)
	}
	var cmd []string
	cmd = append(cmd, RuntimeCommand, "run", "-d")

	if startConfig.Name != "" {
		cmd = append(cmd, "--name", startConfig.Name)
	}
	if startConfig.Privileged {
		cmd = append(cmd, "--privileged")
	}
	if startConfig.NetworkMode != "" {
		cmd = append(cmd, "--network", startConfig.NetworkMode)
	}

	for target, source := range startConfig.Mounts {
		var mountFlag = ""
		if source == "" {
			mountFlag = fmt.Sprintf("%s", target)
		} else {
			mountFlag = fmt.Sprintf("%s:%s", source, target)
		}
		cmd = append(cmd, "-v", mountFlag)
	}

	for k, v := range startConfig.Env {
		envFlag := fmt.Sprintf("%s=%s", k, v)
		cmd = append(cmd, "-e", envFlag)
	}

	cmd = append(cmd, startConfig.Image)
	cmd = append(cmd, startConfig.Command...)

	output, err := e.Exec(cmd...)
	if err != nil {
		return "", fmt.Errorf("error running docker run: %s\nOutput: %s", err, output)
	}

	containerID := strings.TrimSpace(string(output))

	return containerID, nil
}

func (e *executor) IsContainerFoundFiltered(containerID, filter string) (bool, error) {
	if e.containerExec != nil {
		return e.containerExec.IsContainerFoundFiltered(containerID, filter)
	}
	cmd := []string{
		RuntimeCommand, "ps", "-qa",
		"--filter", "id=" + containerID,
		"--filter", filter,
	}

	output, err := e.Exec(cmd...)
	if err != nil {
		return false, err
	}
	outLines := strings.Split(output, "\n")
	lastLine := outLines[len(outLines)-1]
	if lastLine == ContainerShortID(containerID) {
		return true, nil
	}

	return false, nil
}

func (e *executor) GetContainerHealthCheck(containerName string) (string, error) {
	if e.containerExec != nil {
		return e.containerExec.GetContainerHealthCheck(containerName)
	}
	cmd := []string{RuntimeCommand, "inspect", "-f", "'{{ .Config.Healthcheck }}'", containerName}
	output, err := e.Exec(cmd...)
	if err != nil {
		return "", err
	}

	outLines := strings.Split(output, "\n")
	lastLine := outLines[len(outLines)-1]

	// Clearly no HealthCheck section
	if lastLine == "<nil>" {
		return "", nil
	}
	return lastLine, nil
}

func (e *executor) ExecContainer(containerName string, command []string) (string, error) {
	if e.containerExec != nil {
		return e.containerExec.ExecContainer(containerName, command)
	}
	cmd := []string{RuntimeCommand, "exec", containerName}
	cmd = append(cmd, command...)
	return e.Exec(cmd...)
}

// KillContainer runs the kill operation on the provided container name
func (e *executor) KillContainer(name string) (string, error) {
	if e.containerExec != nil {
		return "", e.containerExec.KillContainer(name)
	}
	return e.ExecWithErrorCheck(containerErrorCheckFunction(name, "kill"), RuntimeCommand, "kill", name)
}

// RemoveContainer runs the remove operation on the provided container name
func (e *executor) RemoveContainer(name string) (string, error) {
	if e.containerExec != nil {
		return "", e.containerExec.RemoveContainer(name)
	}
	return e.ExecWithErrorCheck(containerErrorCheckFunction(name, "remove"), RuntimeCommand, "rm", name)
}

// StopContainer runs the stop operation on the provided container name
func (e *executor) StopContainer(name string) (string, error) {
	if e.containerExec != nil {
		return "", e.containerExec.StopContainer(name)
	}
	return e.ExecWithErrorCheck(containerErrorCheckFunction(name, "stop"), RuntimeCommand, "stop", name)
}

func (e *executor) GetContainerIP(containerName string) (string, error) {
	if e.containerExec != nil {
		return e.containerExec.GetContainerIP(containerName)
	}
	stdoutStderr, err := e.Exec(RuntimeCommand, "inspect", "--format='{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'", containerName)
	return strings.Replace(string(stdoutStderr), "'", "", -1), err
}

func (e *executor) GetContainerPort(containerName string) (string, error) {
	if e.containerExec != nil {
		return e.containerExec.GetContainerPort(containerName)
	}
	stdoutStderr, err := e.Exec(RuntimeCommand, "inspect", "--format='{{json .NetworkSettings.Ports}}'", containerName)
	if err != nil {
		return "", err
	}
	rawString := strings.Trim(string(stdoutStderr), "'\n")
	var portMap map[string]interface{}
	err = json.Unmarshal([]byte(rawString), &portMap)
	if err != nil {
		return "", err
	}

	for k := range portMap {
		return strings.Split(k, "/")[0], nil
	}

	return "", fmt.Errorf("no port mapping found: %v %v", rawString, portMap)
}

func (e *executor) GetContainerLogs(containerName string) (string, error) {
	if e.containerExec != nil {
		return e.containerExec.GetContainerLogs(containerName)
	}
	logs, err := e.Exec(RuntimeCommand, "logs", containerName)
	if err != nil {
		log.Error("error getting logs for container %s: %v\n", containerName, err)
		return "", err
	}
	return logs, nil
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
