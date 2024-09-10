package executor

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"strings"
	"time"

	"github.com/docker/docker/api/types"
	"github.com/docker/docker/api/types/container"
	"github.com/docker/docker/api/types/filters"
	"github.com/docker/docker/api/types/image"
	"github.com/docker/docker/client"
	"github.com/docker/docker/pkg/stdcopy"
	"github.com/pkg/errors"
	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/log"
)

type dockerAPIExecutor struct {
	client     *client.Client
	registries map[string]dockerRegistryConfig
}

func newDockerAPIExecutor() (*dockerAPIExecutor, error) {
	cli, err := client.NewClientWithOpts(client.FromEnv, client.WithAPIVersionNegotiation())
	if err != nil {
		return nil, err
	}
	registryConfigs, err := readRegistryConfigs()
	if err != nil {
		return nil, err
	}
	return &dockerAPIExecutor{
		client:     cli,
		registries: registryConfigs,
	}, nil
}

func convertMapToSlice(env map[string]string) []string {
	var result []string
	for k, v := range env {
		result = append(result, k+"="+v)
	}
	return result
}

func (d *dockerAPIExecutor) IsContainerFoundFiltered(containerID, filter string) (bool, error) {
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

	containers, err := d.client.ContainerList(context.Background(), container.ListOptions{
		Filters: filterArgs,
		All:     true,
	})
	if err != nil {
		return false, err
	}

	found := false
	for _, c := range containers {
		if common.ContainerShortID(containerID) == common.ContainerShortID(c.ID) {
			found = true
		}
	}
	log.Debug("checked %s for filter %s (%t)\n", common.ContainerShortID(containerID), filter, found)
	return found, nil
}

func (d *dockerAPIExecutor) GetContainerHealthCheck(containerID string) (string, error) {
	inspectResp, err := d.inspectContainer(containerID)
	if err != nil {
		return "", fmt.Errorf("error inspecting container: %w", err)
	}
	if inspectResp.Config.Healthcheck == nil {
		return "", fmt.Errorf("container %s does not have a health check", containerID)
	}

	log.Trace("%s has healthcheck: %s\n", containerID, strings.Join(inspectResp.Config.Healthcheck.Test, " "))
	return strings.Join(inspectResp.Config.Healthcheck.Test, " "), nil
}

func (d *dockerAPIExecutor) StartContainer(startConfig config.ContainerStartConfig) (string, error) {
	ctx := context.Background()
	var binds []string
	volumes := map[string]struct{}{}

	for containerPath, hostPath := range startConfig.Mounts {
		if hostPath == "" {
			volumes[containerPath] = struct{}{}
		} else {
			bind := fmt.Sprintf("%s:%s", hostPath, containerPath)
			binds = append(binds, bind)
		}
	}

	containerConfig := &container.Config{
		Image:   startConfig.Image,
		Env:     convertMapToSlice(startConfig.Env),
		Volumes: volumes,
	}
	if len(startConfig.Entrypoint) > 0 {
		containerConfig.Entrypoint = startConfig.Entrypoint
	}
	if len(startConfig.Command) > 0 {
		containerConfig.Cmd = startConfig.Command
	}

	hostConfig := &container.HostConfig{
		NetworkMode: container.NetworkMode(startConfig.NetworkMode),
		Privileged:  startConfig.Privileged,
		Binds:       binds,
	}
	resp, err := d.client.ContainerCreate(ctx, containerConfig, hostConfig, nil, nil, startConfig.Name)
	if err != nil {
		return "", errors.Wrapf(err, "create %s", startConfig.Name)
	}
	if err := d.client.ContainerStart(ctx, resp.ID, container.StartOptions{}); err != nil {
		return "", errors.Wrapf(err, "start %s", startConfig.Name)
	}

	log.Info("start %s with %s (%s)\n",
		startConfig.Name, startConfig.Image, common.ContainerShortID(resp.ID))
	return resp.ID, nil
}

func (d *dockerAPIExecutor) ExecContainer(containerName string, command []string) (string, error) {
	ctx := context.Background()
	execConfig := types.ExecConfig{
		AttachStdout: true,
		AttachStderr: true,
		Cmd:          command,
	}

	resp, err := d.client.ContainerExecCreate(ctx, containerName, execConfig)
	if err != nil {
		return "", fmt.Errorf("error creating Exec: %w", err)
	}

	execStartCheck := types.ExecStartCheck{Detach: false, Tty: false}
	attachResp, err := d.client.ContainerExecAttach(ctx, resp.ID, execStartCheck)
	if err != nil {
		return "", fmt.Errorf("error attaching to Exec: %w", err)
	}
	defer attachResp.Close()

	var stdoutBuf, stderrBuf bytes.Buffer
	_, err = stdcopy.StdCopy(&stdoutBuf, &stderrBuf, attachResp.Reader)
	if err != nil {
		return "", fmt.Errorf("error reading Exec output: %w", err)
	}

	execInspect, err := d.client.ContainerExecInspect(ctx, resp.ID)
	if err != nil {
		return "", fmt.Errorf("error inspecting Exec: %w", err)
	}
	log.Info("exec %s %v (exitCode=%d, outBytes=%d)\n",
		containerName, command, execInspect.ExitCode, stdoutBuf.Len())
	return stdoutBuf.String(), nil
}

func (d *dockerAPIExecutor) GetContainerLogs(containerID string) (string, error) {
	timeoutContext, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()
	logsReader, err := d.client.ContainerLogs(timeoutContext, containerID,
		container.LogsOptions{ShowStdout: true, ShowStderr: true})
	if err != nil {
		return "", fmt.Errorf("error getting container logs: %w", err)
	}
	defer logsReader.Close()

	var sbStdOut, sbStdErr strings.Builder
	// if container doesn't have TTY (c.Config.Tty), output is multiplexed
	if _, err := stdcopy.StdCopy(&sbStdOut, &sbStdErr, logsReader); err != nil {
		return "", fmt.Errorf("error copying logs: %w", err)
	}
	log.Info("logs %s (%d bytes stdout, %d bytes stderr)", containerID, sbStdOut.Len(), sbStdErr.Len())
	if sbStdErr.Len() > 0 {
		if sbStdOut.Len() > 0 {
			// not implemented
			return "", errors.New("unhandled container output to stdout and stderr")
		}
		return sbStdErr.String(), nil
	}
	return sbStdOut.String(), nil
}

func (d *dockerAPIExecutor) KillContainer(containerID string) (string, error) {
	timeoutContext, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	err := d.client.ContainerKill(timeoutContext, containerID, "KILL")
	if err != nil {
		return "", fmt.Errorf("error killing container: %w", err)
	}
	log.Debug("kill %s\n", containerID)
	return "", nil
}

func (d *dockerAPIExecutor) StopContainer(name string) (string, error) {
	stopOptions := container.StopOptions{
		Signal:  "SIGTERM",
		Timeout: nil, // 10 secs
	}
	timeoutContext, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	err := d.client.ContainerStop(timeoutContext, name, stopOptions)
	if err != nil {
		return "", fmt.Errorf("error stopping container: %w", err)
	}
	log.Debug("stop %s\n", name)
	return "", nil
}

func (d *dockerAPIExecutor) RemoveContainer(cf ContainerFilter) (string, error) {
	defer d.client.Close()
	timeoutContext, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	removeOptions := container.RemoveOptions{
		RemoveVolumes: true,
		Force:         true,
	}
	err := d.client.ContainerRemove(timeoutContext, cf.Name, removeOptions)
	if err != nil {
		return "", fmt.Errorf("error removing container: %w", err)
	}
	log.Debug("remove %s\n", cf.Name)
	return "", nil
}

func (d *dockerAPIExecutor) inspectContainer(containerID string) (types.ContainerJSON, error) {
	containerJSON, err := d.client.ContainerInspect(context.Background(), containerID)
	if err != nil {
		return types.ContainerJSON{}, fmt.Errorf("error inspecting container: %w", err)
	}
	log.Debug("inspect %s\n", containerID)
	return containerJSON, nil
}

func (d *dockerAPIExecutor) ExitCode(cf ContainerFilter) (int, error) {
	inspectResp, err := d.inspectContainer(cf.Name)
	if err != nil {
		return -1, err
	}
	log.Info("%s exitcode=%s\n", cf.Name, inspectResp.State.ExitCode)
	return inspectResp.State.ExitCode, nil
}

func (d *dockerAPIExecutor) GetContainerIP(containerID string) (string, error) {
	inspectResp, err := d.inspectContainer(containerID)
	if err != nil {
		return "", err
	}
	log.Info("IP for %s is %s\n", containerID,
		inspectResp.NetworkSettings.DefaultNetworkSettings.IPAddress)
	return inspectResp.NetworkSettings.DefaultNetworkSettings.IPAddress, nil
}

func (d *dockerAPIExecutor) GetContainerPort(containerID string) (string, error) {
	containerPort := "-1"
	containerJSON, err := d.inspectContainer(containerID)
	if err != nil {
		return containerPort, err
	}
	for portStr := range containerJSON.NetworkSettings.Ports {
		portSplit := strings.Split(string(portStr), "/")
		if len(portSplit) > 0 {
			containerPort = portSplit[0]
		}
	}
	if containerPort == "-1" {
		return containerPort, log.Error("port not found for container %s", containerID)
	}
	log.Info("port for %s is %s\n", containerID, containerPort)
	return containerPort, nil
}

func (d *dockerAPIExecutor) CheckContainerHealthy(containerID string) (bool, error) {
	containerJSON, err := d.inspectContainer(containerID)
	if err != nil {
		return false, err
	}
	log.Trace("%s is %v\n", containerID, containerJSON.State.Health.Status)
	return containerJSON.State.Health.Status == "healthy", nil
}

func (d *dockerAPIExecutor) IsContainerRunning(containerID string) (bool, error) {
	containerJSON, err := d.inspectContainer(containerID)
	if err != nil {
		return false, err
	}
	log.Trace("%s running: %v\n", containerID, containerJSON.State.Running)
	return containerJSON.State.Running, nil
}

func (d *dockerAPIExecutor) ContainerExists(cf ContainerFilter) (bool, error) {
	_, err := d.inspectContainer(cf.Name)
	if err != nil {
		log.Trace("%s does not exist\n", cf.Name)
		return false, err
	}
	log.Trace("%s exists\n", cf.Name)
	return true, nil
}

func (d *dockerAPIExecutor) PullImage(ref string) error {
	imgFilter := filters.NewArgs(filters.KeyValuePair{
		Key:   "reference",
		Value: ref,
	})
	images, err := d.client.ImageList(context.Background(), image.ListOptions{
		Filters: imgFilter,
	})
	if err != nil {
		return errors.Wrapf(err, "%s", ref)
	}

	if len(images) != 0 {
		log.Trace("%s already exists", ref)
		return nil
	}

	var reader io.ReadCloser
	var pullOptions image.PullOptions
	imageRegistry, _, _ := getFullImageRef(ref)
	if registry, ok := d.registries[imageRegistry]; ok {
		pullOptions = image.PullOptions{RegistryAuth: registry.encodedAuthConfig}
		err = registryLoginAndEnable(d.client, &registry)
		if err != nil {
			return log.Error("registry error: %s", err)
		}
	}
	log.Info("pulling %s", ref)
	reader, err = d.client.ImagePull(context.Background(), ref, pullOptions)
	if err != nil {
		return errors.Wrapf(err, "%s", ref)
	}

	defer reader.Close()
	io.Copy(io.Discard, reader)
	return nil
}

func (d *dockerAPIExecutor) GetContainerStat(containerID string) (*ContainerStat, error) {
	stats, err := d.client.ContainerStats(context.Background(), containerID, false)
	if err != nil {
		return nil, err
	}
	defer stats.Body.Close()
	decoder := json.NewDecoder(stats.Body)
	var statsJSON types.StatsJSON
	if err := decoder.Decode(&statsJSON); err != nil {
		return nil, err
	}
	return ToContainerStat(&statsJSON)
}
