package collector

import (
	"encoding/json"
	"fmt"
	"strings"

	"golang.org/x/exp/maps"

	"github.com/hashicorp/go-multierror"
	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/executor"
)

type DockerCollectorManager struct {
	executor      executor.Executor
	mounts        map[string]string
	env           map[string]string
	config        map[string]any
	bootstrapOnly bool
	testName      string

	CollectorOutput string
	containerID     string
}

func NewDockerCollectorManager(e executor.Executor, name string) *DockerCollectorManager {
	collectorOptions := config.CollectorInfo()

	env := map[string]string{
		"GRPC_SERVER":                   "localhost:9999",
		"ENABLE_CORE_DUMP":              "true",
		"ROX_COLLECTOR_LOG_LEVEL":       collectorOptions.LogLevel,
		"ROX_COLLECTOR_SCRAPE_DISABLED": "true",
		"ROX_COLLECTOR_SCRAPE_INTERVAL": "2",
	}

	mounts := map[string]string{
		"/host/proc:ro":             "/proc",
		"/host/etc:ro":              "/etc",
		"/host/usr/lib:ro":          "/usr/lib",
		"/host/sys/kernel/debug:ro": "/sys/kernel/debug",
		"/etc/stackrox:ro":          "/tmp/collector-test",
	}

	return &DockerCollectorManager{
		executor:      e,
		bootstrapOnly: false,
		env:           env,
		mounts:        mounts,
		config:        map[string]any{},
		testName:      name,
	}
}

func (c *DockerCollectorManager) Setup(options *StartupOptions) error {
	if options == nil {
		// default to empty, if no options are provided (i.e. use the
		// default values)
		options = &StartupOptions{}
	}

	if options.Env != nil {
		maps.Copy(c.env, options.Env)
	}

	if options.Mounts != nil {
		maps.Copy(c.mounts, options.Mounts)
	}

	if options.Config != nil {
		maps.Copy(c.config, options.Config)
	}

	return c.executor.PullImage(config.Images().CollectorImage())
}

func (c *DockerCollectorManager) Launch() error {
	return c.launchCollector()
}

func (c *DockerCollectorManager) TearDown() error {
	isRunning, err := c.IsRunning()
	if err != nil {
		return fmt.Errorf("Unable to check if container is running: %s", err)
	}

	if !isRunning {
		logs, logsErr := c.captureLogs("collector")

		// Check if collector container segfaulted or exited with error
		exitCode, err := c.executor.ExitCode(executor.ContainerFilter{
			Name: "collector",
		})
		if err != nil || exitCode != 0 {
			logsEnd := ""
			if logsErr == nil {
				logsSplit := strings.Split(logs, "\n")
				logsEnd = fmt.Sprintf("\ncollector logs:\n%s\n",
					strings.Join(logsSplit[max(0, len(logsSplit)-24):], "\n"))
			}
			if err != nil {
				return fmt.Errorf("Failed to get container exit code%s: %w", logsEnd, err)
			}
			return fmt.Errorf("Collector container has non-zero exit code (%d)%s", exitCode, logsEnd)
		}
	} else {
		c.stopContainer("collector")
		c.captureLogs("collector")
		c.killContainer("collector")
	}

	return nil
}

func (c *DockerCollectorManager) IsRunning() (bool, error) {
	return c.executor.IsContainerRunning("collector")
}

func (c *DockerCollectorManager) createCollectorStartConfig() (config.ContainerStartConfig, error) {
	startConfig := config.ContainerStartConfig{
		Name:        "collector",
		Image:       config.Images().CollectorImage(),
		Privileged:  true,
		NetworkMode: "host",
		Mounts:      c.mounts,
		Env:         c.env,
	}

	configJson, err := json.Marshal(c.config)
	if err != nil {
		return config.ContainerStartConfig{}, err
	}
	startConfig.Env["COLLECTOR_CONFIG"] = string(configJson)

	if c.bootstrapOnly {
		startConfig.Command = []string{"exit", "0"}
	}
	return startConfig, nil
}

func (c *DockerCollectorManager) launchCollector() error {
	startConfig, err := c.createCollectorStartConfig()
	if err != nil {
		return err
	}
	output, err := c.executor.StartContainer(startConfig)
	c.CollectorOutput = output
	if err != nil {
		return err
	}
	outLines := strings.Split(output, "\n")
	c.containerID = common.ContainerShortID(string(outLines[len(outLines)-1]))
	return err
}

func (c *DockerCollectorManager) captureLogs(containerName string) (string, error) {
	return c.executor.CaptureLogs(c.testName, containerName)
}

func (c *DockerCollectorManager) killContainer(name string) error {
	_, err1 := c.executor.KillContainer(name)
	_, err2 := c.executor.RemoveContainer(executor.ContainerFilter{
		Name: name,
	})

	var result error
	if err1 != nil {
		result = multierror.Append(result, err1)
	}
	if err2 != nil {
		result = multierror.Append(result, err2)
	}

	return result
}

func (c *DockerCollectorManager) stopContainer(name string) error {
	_, err := c.executor.StopContainer(name)
	return err
}

func (c *DockerCollectorManager) ContainerID() string {
	return c.containerID
}

func (c *DockerCollectorManager) TestName() string {
	return c.testName
}

func (c *DockerCollectorManager) SetTestName(testName string) {
	c.testName = testName
}
