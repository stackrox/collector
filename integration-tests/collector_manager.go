package integrationtests

import (
	"fmt"
	"io/ioutil"
	"os"
	"os/user"
	"path/filepath"
	"strings"

	"github.com/hashicorp/go-multierror"

	"github.com/boltdb/bolt"
)

type collectorManager struct {
	executor          Executor
	Mounts            map[string]string
	Env               map[string]string
	DBPath            string
	CollectorOutput   string
	CollectorImage    string
	GRPCServerImage   string
	DisableGrpcServer bool
	BootstrapOnly     bool
	TestName          string
	CoreDumpFile      string
}

func NewCollectorManager(e Executor, name string) *collectorManager {
	var collectorPreArguments = os.Getenv("COLLECTOR_PRE_ARGUMENTS")
	collectionMethod := ReadEnvVarWithDefault("COLLECTION_METHOD", "kernel_module")
	if strings.Contains(collectionMethod, "module") {
		collectionMethod = "kernel_module"
	}

	offlineMode := ReadBoolEnvVar("COLLECTOR_OFFLINE_MODE")

	env := map[string]string{
		"GRPC_SERVER":                      "localhost:9999",
		"COLLECTOR_CONFIG":                 `{"logLevel":"debug","turnOffScrape":true,"scrapeInterval":2}`,
		"COLLECTION_METHOD":                collectionMethod,
		"COLLECTOR_PRE_ARGUMENTS":          collectorPreArguments,
		"ENABLE_CORE_DUMP":                 "true",
	}
	if !offlineMode {
		env["MODULE_DOWNLOAD_BASE_URL"] = "https://collector-modules.stackrox.io/612dd2ee06b660e728292de9393e18c81a88f347ec52a39207c5166b5302b656"
	}
	mounts := map[string]string{
		"/host/var/run/docker.sock:ro": "/var/run/docker.sock",
		"/host/proc:ro":                "/proc",
		"/host/etc:ro":                 "/etc/",
		"/host/usr/lib:ro":             "/usr/lib/",
		"/host/sys:ro":                 "/sys/",
		"/host/dev:ro":                 "/dev",
		"/tmp":				"/tmp",
		// /module is an anonymous volume to reflect the way collector
		// is usually run in kubernetes (with in-memory volume for /module)
		"/module": "",
	}

	collectorImage := ReadEnvVar("COLLECTOR_IMAGE")

	return &collectorManager{
		DBPath:            "/tmp/collector-test.db",
		executor:          e,
		DisableGrpcServer: false,
		BootstrapOnly:     false,
		CollectorImage:    collectorImage,
		GRPCServerImage:   "stackrox/grpc-server:3.68.x-35-g12796a7c2d",
		Env:               env,
		Mounts:            mounts,
		TestName:          name,
		CoreDumpFile:      "/tmp/core.out",
	}
}

func (c *collectorManager) Setup() error {
	if err := c.executor.PullImage(c.CollectorImage); err != nil {
		return err
	}

	if !c.DisableGrpcServer {
		if err := c.executor.PullImage(c.GRPCServerImage); err != nil {
			return err
		}

		// remove previous db file
		c.executor.Exec("rm", "-fv", c.DBPath)
	}
	return nil
}

func (c *collectorManager) Launch() error {
	if !c.DisableGrpcServer {
		err := c.launchGRPCServer()
		if err != nil {
			return err
		}
	}
	return c.launchCollector()
}

func (c *collectorManager) TearDown() error {
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
	if !c.DisableGrpcServer {
		c.captureLogs("grpc-server")
		if _, err := c.executor.CopyFromHost(c.DBPath, c.DBPath); err != nil {
			return err
		}
		c.killContainer("grpc-server")
	}
	return nil
}

func (c *collectorManager) BoltDB() (db *bolt.DB, err error) {
	opts := &bolt.Options{ReadOnly: true}
	db, err = bolt.Open(c.DBPath, 0600, opts)
	if err != nil {
		fmt.Printf("Permission error. %v\n", err)
	}
	return db, err
}

//These two methods might be useful in the future. I used them for debugging
func (c *collectorManager) getContainers() (string, error) {
	cmd := []string{"docker", "container", "ps"}
	containers, err := c.executor.Exec(cmd...)

	return containers, err
}

func (c *collectorManager) getAllContainers() (string, error) {
	cmd := []string{"docker", "container", "ps", "-a"}
	containers, err := c.executor.Exec(cmd...)

	return containers, err
}

func (c *collectorManager) launchGRPCServer() error {
	user, _ := user.Current()
	selinuxErr := setSelinuxPermissiveIfNeeded()
	if selinuxErr != nil {
		return selinuxErr
	}
	cmd := []string{"docker", "run",
		"-d",
		"--rm",
		"--name", "grpc-server",
		"--network=host",
		"-v", "/tmp:/tmp:rw",
		"--user", user.Uid + ":" + user.Gid,
		c.GRPCServerImage,
	}
	_, err := c.executor.Exec(cmd...)
	return err
}

func (c *collectorManager) launchCollector() error {
	coreDumpErr := c.SetCoreDumpPath(c.CoreDumpFile)
	if coreDumpErr != nil {
		return coreDumpErr
	}

	cmd := []string{"docker", "run",
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

func (c *collectorManager) captureLogs(containerName string) (string, error) {
	logs, err := c.executor.Exec("docker", "logs", containerName)
	if err != nil {
		fmt.Printf("docker logs error (%v) for container %s\n", err, containerName)
		return "", err
	}
	logDirectory := filepath.Join(".", "container-logs")
	os.MkdirAll(logDirectory, os.ModePerm)
	logFile := filepath.Join(logDirectory, strings.ReplaceAll(c.TestName, "/", "_")+"-"+containerName+".log")
	err = ioutil.WriteFile(logFile, []byte(logs), 0644)
	if err != nil {
		return "", err
	}
	return logs, nil
}

func (c *collectorManager) killContainer(name string) error {
	_, err1 := c.executor.Exec("docker", "kill", name)
	_, err2 := c.executor.Exec("docker", "rm", "-fv", name)

	var result error
	if err1 != nil {
		result = multierror.Append(result, err1)
	}
	if err2 != nil {
		result = multierror.Append(result, err2)
	}

	return result
}

func (c *collectorManager) stopContainer(name string) error {
	_, err := c.executor.Exec("docker", "stop", "--time", "100", name)
	return err
}

// Sets the path to where core dumps are saved to. This is specified in the core_pattern file
// The core_pattern file is backed up, because we don't want to permanently change it
func (c *collectorManager) SetCoreDumpPath(coreDumpFile string) error {
	remote_host_type := ReadEnvVarWithDefault("REMOTE_HOST_TYPE", "local")
	if remote_host_type != "local" {
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
func (c *collectorManager) RestoreCoreDumpPath() error {
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
func (c *collectorManager) GetCoreDump(coreDumpFile string) error {
	if ReadEnvVarWithDefault("REMOTE_HOST_TYPE", "local") != "local" {
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
