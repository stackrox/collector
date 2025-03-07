package executor

import (
	"context"
	"fmt"
	"os"
	"strings"
	"time"

	trace_noop "go.opentelemetry.io/otel/trace/noop"
	internalapi "k8s.io/cri-api/pkg/apis"
	pb "k8s.io/cri-api/pkg/apis/runtime/v1"
	cri_client "k8s.io/cri-client/pkg"

	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/log"
)

type criExecutor struct {
	imageService   internalapi.ImageManagerService
	runtimeService internalapi.RuntimeService
	registries     map[string]dockerRegistryConfig
}

func newCriExecutor() (*criExecutor, error) {
	socket := config.RuntimeInfo().Socket
	imageService, err := cri_client.NewRemoteImageService(socket, 1*time.Minute, trace_noop.NewTracerProvider(), nil)
	if err != nil {
		return nil, err
	}

	runtimeService, err := cri_client.NewRemoteRuntimeService(socket, 1*time.Minute, trace_noop.NewTracerProvider(), nil)
	if err != nil {
		return nil, err
	}

	regs, err := readRegistryConfigs()
	if err != nil {
		return nil, err
	}

	return &criExecutor{
		imageService:   imageService,
		runtimeService: runtimeService,
		registries:     regs,
	}, nil
}

func (c *criExecutor) PullImage(image string) error {
	var auth pb.AuthConfig
	imageRegistry, _, _ := getFullImageRef(image)
	registry, ok := c.registries[imageRegistry]
	if ok {
		auth.Username = registry.authConfig.Username
		auth.Password = registry.authConfig.Password
		auth.Auth = registry.authConfig.Auth
	}

	imageSpec := pb.ImageSpec{
		Image: image,
	}

	_, err := c.imageService.PullImage(context.Background(), &imageSpec, &auth, nil)
	return err
}

func (c *criExecutor) IsContainerRunning(container string) (bool, error) {
	status, err := c.getStatus(container)
	if err != nil {
		return false, err
	}
	return status.State == pb.ContainerState_CONTAINER_RUNNING, nil
}

func (c *criExecutor) ContainerExists(filter ContainerFilter) (bool, error) {
	_, err := c.getStatus(filter.Name)
	if err != nil {
		return false, err
	}
	return true, nil
}

func (c *criExecutor) ExitCode(filter ContainerFilter) (int, error) {
	status, err := c.getStatus(filter.Name)
	if err != nil {
		return -1, err
	}
	return int(status.GetExitCode()), nil
}

func (c *criExecutor) ExecContainer(opts *ExecOptions) (string, error) {
	id, err := c.getContainerId(opts.ContainerName)
	if err != nil {
		return "", err
	}

	if opts.Detach {
		// Some commands run processes in the background by doing something
		// like '/bin/sh -c "comm &". In docker and podman, the shell exits
		// after detaching the background process and returns control to the
		// tests. On the other hand, containerd waits for the background
		// process to end and hangs the tests. Best thing I could figure out
		// to work around this is to throw the command into a goroutine.
		// Once the container being exec'ed into exits, this goroutine will
		// finish with an error, which we promptly ignore.
		go c.runtimeService.ExecSync(context.Background(), id, opts.Command, 1*time.Minute)
		return "", nil
	}

	stdout, _, err := c.runtimeService.ExecSync(context.Background(), id, opts.Command, 1*time.Minute)
	if err != nil {
		return "", err
	}
	return string(stdout), nil
}

func (c *criExecutor) KillContainer(name string) (string, error) {
	return c.stopContainer(name, 0)
}

func (c *criExecutor) StopContainer(name string) (string, error) {
	return c.stopContainer(name, 10)
}

func (c *criExecutor) stopContainer(name string, timeout int64) (string, error) {
	id, err := c.getContainerId(name)
	if err != nil {
		return "", err
	}
	return "", c.runtimeService.StopContainer(context.Background(), id, timeout)
}

func (c *criExecutor) RemoveContainer(filter ContainerFilter) (string, error) {
	// crictl keeps the logs for a container and appends to it if another
	// container with the same log path and name is created, so we
	// explicitly delete the file here.
	os.Remove(fmt.Sprintf("/tmp/collector-integration-tests/%s", filter.Name))

	container, err := c.getContainer(filter.Name)
	if err != nil {
		return "", err
	}

	return "", c.runtimeService.RemovePodSandbox(context.Background(), container.GetPodSandboxId())
}

func (c *criExecutor) StartContainer(config config.ContainerStartConfig) (string, error) {
	ctx := context.Background()
	labels := map[string]string{"app": config.Name}

	// CRI doesn't show exposed ports, so in order to know which port a
	// container uses, we add a label to it.
	if len(config.Ports) != 0 {
		labels["ports"] = strings.Trim(strings.Join(strings.Fields(fmt.Sprint(config.Ports)), ","), "[]")
	}

	network := pb.NamespaceMode_POD
	if config.NetworkMode == "host" {
		network = pb.NamespaceMode_NODE
	}

	sandboxConfig := pb.PodSandboxConfig{
		Metadata: &pb.PodSandboxMetadata{
			Name:      config.Name,
			Uid:       "uid",
			Namespace: "collector-tests",
		},
		Linux: &pb.LinuxPodSandboxConfig{
			SecurityContext: &pb.LinuxSandboxSecurityContext{
				Privileged: config.Privileged,
				NamespaceOptions: &pb.NamespaceOption{
					Network: network,
				},
			},
		},
		LogDirectory: "/tmp/collector-integration-tests",
		Labels:       labels,
	}

	podId, err := c.runtimeService.RunPodSandbox(ctx, &sandboxConfig, "runc")
	if err != nil {
		return "", err
	}

	envs := []*pb.KeyValue{}
	for k, v := range config.Env {
		envs = append(envs, &pb.KeyValue{
			Key:   k,
			Value: v,
		})
	}

	mounts := []*pb.Mount{}
	for containerPath, hostPath := range config.Mounts {
		readonly := false
		if strings.HasSuffix(containerPath, ":ro") {
			containerPath = strings.TrimSuffix(containerPath, ":ro")
			readonly = true
		}

		mounts = append(mounts, &pb.Mount{
			ContainerPath:     containerPath,
			HostPath:          hostPath,
			Readonly:          readonly,
			RecursiveReadOnly: readonly,
		})
	}

	containerConfig := pb.ContainerConfig{
		Metadata: &pb.ContainerMetadata{Name: config.Name},
		Image:    &pb.ImageSpec{Image: config.Image},
		Envs:     envs,
		Mounts:   mounts,
		Linux: &pb.LinuxContainerConfig{
			SecurityContext: &pb.LinuxContainerSecurityContext{
				Privileged: config.Privileged,
				NamespaceOptions: &pb.NamespaceOption{
					Network: network,
				},
			},
		},
		LogPath: config.Name,
		Labels:  labels,
	}

	if len(config.Entrypoint) > 0 {
		containerConfig.Command = config.Entrypoint
	}
	if len(config.Command) > 0 {
		containerConfig.Args = config.Command
	}

	containerId, err := c.runtimeService.CreateContainer(ctx, podId, &containerConfig, &sandboxConfig)
	if err != nil {
		return "", err
	}

	err = c.runtimeService.StartContainer(ctx, containerId)
	if err != nil {
		return "", err
	}

	_, err = RetryWithTimeout(func() (output string, err error) {
		resp, err := c.runtimeService.ContainerStatus(ctx, containerId, true)
		if err != nil {
			log.Warn("Failed to inspect %s: %+v", config.Name, err)
			return "", err
		}

		status := resp.GetStatus()
		// Container is running or has successfully completed execution
		if status.State == pb.ContainerState_CONTAINER_RUNNING ||
			(status.State == pb.ContainerState_CONTAINER_EXITED && status.ExitCode == 0) {
			return "", nil
		} else {
			return "", fmt.Errorf("Container %s is not running", config.Name)
		}
	}, fmt.Errorf("Container %s didn't start in time", config.Name))

	if err != nil {
		return "", err
	}

	log.Info("start %s with %s\n", config.Name, config.Image)
	return containerId, nil
}

func (c *criExecutor) GetContainerHealthCheck(containerID string) (string, error) {
	return "", fmt.Errorf("Unsupported")
}

func (c *criExecutor) GetContainerIP(name string) (string, error) {
	container, err := c.getContainer(name)
	if err != nil {
		return "", nil
	}

	status, err := c.runtimeService.PodSandboxStatus(context.Background(), container.GetPodSandboxId(), true)
	if err != nil {
		return "", err
	}
	return status.Status.Network.GetIp(), nil
}

func (c *criExecutor) GetContainerPort(name string) (string, error) {
	container, err := c.getContainer(name)
	if err != nil {
		return "", nil
	}

	ports, ok := container.Labels["ports"]
	if !ok {
		return "", log.Error("No port configured in %q", name)
	}
	return strings.Split(ports, ",")[0], nil
}

func (c *criExecutor) GetContainerLogs(containerName string) (ContainerLogs, error) {
	l, err := os.ReadFile(fmt.Sprintf("/tmp/collector-integration-tests/%s", containerName))
	if err != nil {
		return ContainerLogs{}, err
	}

	var out string
	for _, line := range strings.Split(string(l), "\n") {
		// Every line on a CRI log looks like this:
		//   2025-03-04T10:36:02.842353626Z stderr F <Log line>
		// We remove the timestamp, stream and F marker for it to be cleaner.
		dateEnd := strings.Index(line, " ")
		if dateEnd == -1 {
			continue
		}
		line = line[dateEnd+1:]

		streamEnd := strings.Index(line, " ")
		if streamEnd == -1 {
			continue
		}
		line = line[streamEnd+1:]

		markerEnd := strings.Index(line, " ")
		if markerEnd == -1 {
			continue
		}
		out += line[markerEnd+1:] + "\n"
	}

	return ContainerLogs{Stdout: out}, nil
}

func (c *criExecutor) CaptureLogs(testName, containerName string) (string, error) {
	log.Info("%s: Gathering logs for %q", testName, containerName)
	logs, err := c.GetContainerLogs(containerName)
	if err != nil {
		return "", err
	}

	file, err := common.PrepareLog(testName, fmt.Sprintf("%s.log", containerName))
	if err != nil {
		return "", err
	}
	defer file.Close()
	file.WriteString(logs.Stdout)

	return logs.Stdout, nil
}

func (c *criExecutor) IsContainerFoundFiltered(containerID, filter string) (bool, error) {
	parts := strings.SplitN(filter, "=", 2)
	if len(parts) != 2 {
		return false, fmt.Errorf("filter format is invalid")
	}
	filterKey, filterValue := parts[0], parts[1]
	containerFilter := pb.ContainerFilter{}

	switch filterKey {
	case "health":
		// CRI doesn't seem to have a notion of health, so instead we
		// translate it to a state check. Currently we only check for
		// healthy containers, which would translate to a running container.
		var expected pb.ContainerStateValue
		switch filterValue {
		case "healthy":
			expected.State = pb.ContainerState_CONTAINER_RUNNING
		default:
			return false, log.Error("Unsupported status %q", filterValue)
		}
		containerFilter.State = &expected

	case "status":
		var expected pb.ContainerStateValue
		switch filterValue {
		case "created":
			expected.State = pb.ContainerState_CONTAINER_CREATED
		case "running":
			expected.State = pb.ContainerState_CONTAINER_RUNNING
		case "exited":
			expected.State = pb.ContainerState_CONTAINER_EXITED
		case "unknown":
			expected.State = pb.ContainerState_CONTAINER_UNKNOWN
		default:
			return false, log.Error("Unsupported status %q", filterValue)
		}
		containerFilter.State = &expected
	default:
		return false, log.Error("Unsupported filter key %q", filterKey)
	}

	containers, err := c.runtimeService.ListContainers(context.Background(), &containerFilter)
	if err != nil {
		return false, err
	}

	for _, c := range containers {
		if containerID == c.Id {
			return true, nil
		}
	}
	return false, nil
}

func (c *criExecutor) getStatus(container string) (*pb.ContainerStatus, error) {
	id, err := c.getContainerId(container)
	if err != nil {
		return nil, err
	}

	status, err := c.runtimeService.ContainerStatus(context.Background(), id, true)
	if err != nil {
		return nil, err
	}
	return status.GetStatus(), nil
}

func (c *criExecutor) getContainerId(name string) (string, error) {
	container, err := c.getContainer(name)
	if err != nil {
		return "", err
	}

	return common.ContainerShortID(container.GetId()), nil
}

func (c *criExecutor) getContainer(name string) (*pb.Container, error) {
	containers, err := c.runtimeService.ListContainers(context.Background(), &pb.ContainerFilter{
		LabelSelector: map[string]string{"app": name},
	})
	if err != nil {
		return nil, err
	}
	if len(containers) != 1 {
		return nil, log.Error("Meant to find a single container with label 'app=%s', got=%d", name, len(containers))
	}
	return containers[0], nil
}
