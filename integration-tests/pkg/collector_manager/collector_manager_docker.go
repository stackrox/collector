package collector_manager

import (
	"encoding/json"
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"
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

func newDockerManager(e executor.Executor, name string) *DockerCollectorManager {
	collectorOptions := config.CollectorInfo()

	collectionMethod := config.CollectionMethod()

	collectorConfig := map[string]any{
		"logLevel":       collectorOptions.LogLevel,
		"turnOffScrape":  true,
		"scrapeInterval": 2,
	}

	env := map[string]string{
		"GRPC_SERVER":             "localhost:9999",
		"COLLECTION_METHOD":       collectionMethod,
		"COLLECTOR_PRE_ARGUMENTS": collectorOptions.PreArguments,
		"ENABLE_CORE_DUMP":        "true",
	}

	if !collectorOptions.Offline {
		env["MODULE_DOWNLOAD_BASE_URL"] = "https://collector-modules.stackrox.io/612dd2ee06b660e728292de9393e18c81a88f347ec52a39207c5166b5302b656"
	}

	mounts := map[string]string{
		"/host/proc:ro":             "/proc",
		"/host/etc:ro":              "/etc",
		"/host/usr/lib:ro":          "/usr/lib",
		"/host/sys/kernel/debug:ro": "/sys/kernel/debug",
		"/tmp":                      "/tmp",
		// /module is an anonymous volume to reflect the way collector
		// is usually run in kubernetes (with in-memory volume for /module)
		"/module": "",
	}

	return &DockerCollectorManager{
		executor:      e,
		bootstrapOnly: false,
		env:           env,
		mounts:        mounts,
		config:        collectorConfig,
		testName:      name,
	}
}

func (c *DockerCollectorManager) Setup(options *CollectorStartupOptions) error {
	if options == nil {
		// default to empty, if no options are provided (i.e. use the
		// default values)
		options = &CollectorStartupOptions{}
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
		fmt.Println("Error: Checking if container running")
		return err
	}

	if !isRunning {
		c.captureLogs("collector")
		// Check if collector container segfaulted or exited with error
		exitCode, err := c.executor.ExitCode("collector")
		if err != nil {
			fmt.Println("Error: Container not running")
			return err
		}
		if exitCode != 0 {
			return fmt.Errorf("Collector container has non-zero exit code (%d)", exitCode)
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

// These two methods might be useful in the future. I used them for debugging
func (c *DockerCollectorManager) getContainers() (string, error) {
	cmd := []string{executor.RuntimeCommand, "container", "ps"}
	containers, err := c.executor.Exec(cmd...)

	return containers, err
}

func (c *DockerCollectorManager) getAllContainers() (string, error) {
	cmd := []string{executor.RuntimeCommand, "container", "ps", "-a"}
	containers, err := c.executor.Exec(cmd...)

	return containers, err
}

func (c *DockerCollectorManager) launchCollector() error {
	cmd := []string{executor.RuntimeCommand, "run",
		"--name", "collector",
		"--privileged",
		"--network=host"}

	if !c.bootstrapOnly {
		cmd = append(cmd, "-d")
	}

	for dst, src := range c.mounts {
		mount := src + ":" + dst
		if src == "" {
			// allows specification of anonymous volumes
			mount = dst
		}
		cmd = append(cmd, "-v", mount)
	}

	for k, v := range c.env {
		cmd = append(cmd, "--env", k+"="+v)
	}

	configJson, err := json.Marshal(c.config)
	if err != nil {
		return err
	}

	cmd = append(cmd, "--env", "COLLECTOR_CONFIG="+string(configJson))
	cmd = append(cmd, config.Images().CollectorImage())

	if c.bootstrapOnly {
		cmd = append(cmd, "exit", "0")
	}

	output, err := c.executor.Exec(cmd...)
	c.CollectorOutput = output

	outLines := strings.Split(output, "\n")
	c.containerID = common.ContainerShortID(string(outLines[len(outLines)-1]))
	return err
}

func (c *DockerCollectorManager) captureLogs(containerName string) (string, error) {
	logs, err := c.executor.Exec(executor.RuntimeCommand, "logs", containerName)
	if err != nil {
		fmt.Printf(executor.RuntimeCommand+" logs error (%v) for container %s\n", err, containerName)
		return "", err
	}
	logDirectory := filepath.Join(".", "container-logs", config.VMInfo().Config, config.CollectionMethod())
	os.MkdirAll(logDirectory, os.ModePerm)
	logFile := filepath.Join(logDirectory, strings.ReplaceAll(c.testName, "/", "_")+"-"+containerName+".log")
	err = ioutil.WriteFile(logFile, []byte(logs), 0644)
	if err != nil {
		return "", err
	}
	return logs, nil
}

func (c *DockerCollectorManager) killContainer(name string) error {
	_, err1 := c.executor.KillContainer(name)
	_, err2 := c.executor.RemoveContainer(name)

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
