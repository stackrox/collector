package common

import (
	"fmt"
	"io/ioutil"
	"os"

	// "os/user"
	"path/filepath"
	"strings"

	"github.com/hashicorp/go-multierror"

	"github.com/boltdb/bolt"

	"github.com/stackrox/collector/integration-tests/suites/config"
)

type CollectorManager struct {
	executor        Executor
	Mounts          map[string]string
	Env             map[string]string
	DBPath          string
	DBPathRemote    string
	CollectorOutput string
	CollectorImage  string
	GRPCServerImage string
	BootstrapOnly   bool
	TestName        string
	CoreDumpFile    string
	VmConfig        string
}

func NewCollectorManager(e Executor, name string) *CollectorManager {
	collectorOptions := config.CollectorInfo()
	runtimeOptions := config.RuntimeInfo()
	image_store := config.Images()

	collectionMethod := config.CollectionMethod()
	collectorConfig := fmt.Sprintf(`{"logLevel":"%s","turnOffScrape":true,"scrapeInterval":2}`, collectorOptions.LogLevel)

	env := map[string]string{
		"GRPC_SERVER":                     "localhost:9999",
		"COLLECTOR_CONFIG":                collectorConfig,
		"COLLECTION_METHOD":               collectionMethod,
		"COLLECTOR_PRE_ARGUMENTS":         collectorOptions.PreArguments,
		"ENABLE_CORE_DUMP":                "false",
		"ROX_COLLECTOR_CORE_BPF_HARDFAIL": "true",
	}
	if !collectorOptions.Offline {
		env["MODULE_DOWNLOAD_BASE_URL"] = "https://collector-modules.stackrox.io/612dd2ee06b660e728292de9393e18c81a88f347ec52a39207c5166b5302b656"
	}
	mounts := map[string]string{
		// The presence of this socket disables an optimisation, which would turn off podman runtime parsing.
		// https://github.com/falcosecurity/libs/pull/296
		"/run/podman/podman.sock:ro": runtimeOptions.Socket,
		"/host/proc:ro":              "/proc",
		"/host/etc:ro":               "/etc/",
		"/host/usr/lib:ro":           "/usr/lib/",
		"/host/sys:ro":               "/sys/",
		"/host/dev:ro":               "/dev",
		"/tmp":                       "/tmp",
		// /module is an anonymous volume to reflect the way collector
		// is usually run in kubernetes (with in-memory volume for /module)
		"/module": "",
	}

	vm_config := config.VMInfo().Config

	return &CollectorManager{
		DBPathRemote:    "/tmp/collector-test.db",
		DBPath:          "/tmp/collector-test-" + vm_config + "-" + collectionMethod + ".db",
		executor:        e,
		BootstrapOnly:   false,
		CollectorImage:  image_store.CollectorImage(),
		GRPCServerImage: image_store.ImageByKey("grpc-server"),
		Env:             env,
		Mounts:          mounts,
		TestName:        name,
		CoreDumpFile:    "/tmp/core.out",
		VmConfig:        vm_config,
	}
}

func (c *CollectorManager) Setup() error {
	return c.executor.PullImage(c.CollectorImage)
}

func (c *CollectorManager) Launch() error {
	return c.launchCollector()
}

func (c *CollectorManager) TearDown() error {
	coreDumpErr := c.GetCoreDump(c.CoreDumpFile)
	if coreDumpErr != nil {
		return coreDumpErr
	}

	isRunning, err := c.executor.IsContainerRunning("collector")
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

func (c *CollectorManager) BoltDB() (db *bolt.DB, err error) {
	opts := &bolt.Options{ReadOnly: true}
	db, err = bolt.Open(c.DBPath, 0600, opts)
	if err != nil {
		fmt.Printf("Permission error. %v\n", err)
	}
	return db, err
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

func (c *CollectorManager) launchGRPCServer() error {
	// c.Sensor.Start()
	return nil
}

func (c *CollectorManager) launchCollector() error {
	coreDumpErr := c.SetCoreDumpPath(c.CoreDumpFile)
	if coreDumpErr != nil {
		return coreDumpErr
	}

	cmd := []string{RuntimeCommand, "run",
		"--name", "collector",
		"--privileged",
		"--network=host"}

	if !c.BootstrapOnly {
		cmd = append(cmd, "-d")
	}

	for dst, src := range c.Mounts {
		mount := src + ":" + dst
		if src == "" {
			// allows specification of anonymous volumes
			mount = dst
		}
		cmd = append(cmd, "-v", mount)
	}
	for k, v := range c.Env {
		cmd = append(cmd, "--env", k+"="+v)
	}

	cmd = append(cmd, c.CollectorImage)

	if c.BootstrapOnly {
		cmd = append(cmd, "exit", "0")
	}

	output, err := c.executor.Exec(cmd...)
	c.CollectorOutput = output
	return err
}

func (c *CollectorManager) captureLogs(containerName string) (string, error) {
	logs, err := c.executor.Exec(RuntimeCommand, "logs", containerName)
	if err != nil {
		fmt.Printf(RuntimeCommand+" logs error (%v) for container %s\n", err, containerName)
		return "", err
	}
	logDirectory := filepath.Join(".", "container-logs", c.VmConfig, c.Env["COLLECTION_METHOD"])
	os.MkdirAll(logDirectory, os.ModePerm)
	logFile := filepath.Join(logDirectory, strings.ReplaceAll(c.TestName, "/", "_")+"-"+containerName+".log")
	err = ioutil.WriteFile(logFile, []byte(logs), 0644)
	if err != nil {
		return "", err
	}
	return logs, nil
}

func (c *CollectorManager) killContainer(name string) error {
	_, err1 := c.executor.Exec(RuntimeCommand, "kill", name)
	_, err2 := c.executor.Exec(RuntimeCommand, "rm", "-fv", name)

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
	_, err := c.executor.Exec(RuntimeCommand, "stop", "--time", "100", name)
	return err
}

// Sets the path to where core dumps are saved to. This is specified in the core_pattern file
// The core_pattern file is backed up, because we don't want to permanently change it
func (c *CollectorManager) SetCoreDumpPath(coreDumpFile string) error {
	if !config.HostInfo().IsLocal() {
		corePatternFile := "/proc/sys/kernel/core_pattern"
		corePatternBackupFile := "/tmp/core_pattern_backup"
		cmdBackupCorePattern := []string{"sudo", "cp", corePatternFile, corePatternBackupFile}
		cmdSetCoreDumpPath := []string{"echo", "'" + coreDumpFile + "'", "|", "sudo", "tee", corePatternFile}
		var err error
		_, err = c.executor.Exec(cmdBackupCorePattern...)
		if err != nil {
			fmt.Printf("Error: Unable to backup core_pattern file. %v\n", err)
			return err
		}
		_, err = c.executor.Exec(cmdSetCoreDumpPath...)
		if err != nil {
			fmt.Printf("Error: Unable to set core dump file path in core_pattern. %v\n", err)
			return err
		}
	}
	return nil
}

// Restores the backed up core_pattern file, which sets the location where core dumps are written to.
func (c *CollectorManager) RestoreCoreDumpPath() error {
	corePatternFile := "/proc/sys/kernel/core_pattern"
	corePatternBackupFile := "/tmp/core_pattern_backup"
	// cat is used to restore the backup instead of mv, becuase mv is not allowed.
	cmdRestoreCorePattern := []string{"cat", corePatternBackupFile, "|", "sudo", "tee", corePatternFile}
	_, err := c.executor.Exec(cmdRestoreCorePattern...)
	if err != nil {
		fmt.Printf("Error: Unable to restore core dump path. %v\n", err)
		return err
	}
	return nil
}

// If the integration test is run on a remote host the core dump needs to be copied from the remote host
// to the local maching
func (c *CollectorManager) GetCoreDump(coreDumpFile string) error {
	if c.Env["ENABLE_CORE_DUMP"] == "true" && !config.HostInfo().IsLocal() {
		cmd := []string{"sudo", "chmod", "755", coreDumpFile}
		c.executor.Exec(cmd...)
		c.executor.CopyFromHost(coreDumpFile, coreDumpFile)
		err := c.RestoreCoreDumpPath()
		if err != nil {
			return err
		}
	}
	return nil
}
