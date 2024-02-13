package common

import (
	"encoding/json"
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"

	"golang.org/x/exp/maps"

	"github.com/hashicorp/go-multierror"
	"github.com/stackrox/collector/integration-tests/suites/config"
	"github.com/stackrox/collector/integration-tests/suites/log"
)

type CollectorStartupOptions struct {
	Mounts        map[string]string
	Env           map[string]string
	Config        map[string]any
	BootstrapOnly bool
}

type CollectorManager struct {
	executor      Executor
	mounts        map[string]string
	env           map[string]string
	config        map[string]any
	bootstrapOnly bool
	testName      string

	CollectorOutput string
	ContainerID     string
}

func NewCollectorManager(e Executor, name string) *CollectorManager {
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

	return &CollectorManager{
		executor:      e,
		bootstrapOnly: false,
		env:           env,
		mounts:        mounts,
		config:        collectorConfig,
		testName:      name,
	}
}

func (c *CollectorManager) Setup(options *CollectorStartupOptions) error {
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

func (c *CollectorManager) Launch() error {
	return c.launchCollector()
}

func (c *CollectorManager) TearDown() error {
	isRunning, err := c.IsRunning()
	if err != nil {
		log.Error("Checking if container running")
		return err
	}

	if !isRunning {
		c.captureLogs("collector")
		// Check if collector container segfaulted or exited with error
		exitCode, err := c.executor.ExitCode("collector")
		if err != nil {
			log.Error("Container not running")
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

func (c *CollectorManager) IsRunning() (bool, error) {
	return c.executor.IsContainerRunning("collector")
}

// These two methods might be useful in the future. I used them for debugging
func (c *CollectorManager) getContainers() (string, error) {
	cmd := []string{RuntimeCommand, "container", "ps"}
	containers, err := c.executor.Exec(cmd...)

	return containers, err
}

func (c *CollectorManager) getAllContainers() (string, error) {
	cmd := []string{RuntimeCommand, "container", "ps", "-a"}
	containers, err := c.executor.Exec(cmd...)

	return containers, err
}

func (c *CollectorManager) createCollectorStartConfig() (ContainerStartConfig, error) {
	coreDumpErr := c.SetCoreDumpPath(c.coreDumpPath)
	if coreDumpErr != nil {
		return ContainerStartConfig{}, coreDumpErr
	}

	startConfig := ContainerStartConfig{
		Name:        "collector",
		Image:       config.Images().CollectorImage(),
		Privileged:  true,
		NetworkMode: "host",
		Mounts:      c.mounts,
		Env:         c.env,
	}

	configJson, err := json.Marshal(c.config)
	if err != nil {
		return ContainerStartConfig{}, err
	}
	startConfig.Env["COLLECTOR_CONFIG"] = string(configJson)

	if c.bootstrapOnly {
		startConfig.Command = []string{"exit", "0"}
	}

	return startConfig, nil
}

func (c *CollectorManager) launchCollector() error {
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
	c.ContainerID = ContainerShortID(string(outLines[len(outLines)-1]))
	return err
}

func (c *CollectorManager) captureLogs(containerName string) (string, error) {
	logs, err := c.executor.GetContainerLogs(containerName)
	if err != nil {
		log.Error(RuntimeCommand+" logs error (%v) for container %s\n", err, containerName)
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

func (c *CollectorManager) killContainer(name string) error {
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

func (c *CollectorManager) stopContainer(name string) error {
	_, err := c.executor.StopContainer(name)
	return err
}
