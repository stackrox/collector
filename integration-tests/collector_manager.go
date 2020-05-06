package integrationtests

import (
	"fmt"
	"io/ioutil"
	"os"
	"os/user"
	"path/filepath"
	"strings"

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
}

func NewCollectorManager(e Executor, name string) *collectorManager {
	collectionMethod := ReadEnvVarWithDefault("COLLECTION_METHOD", "kernel_module")
	if strings.Contains(collectionMethod, "module") {
		collectionMethod = "kernel_module"
	}

	env := map[string]string{
		"GRPC_SERVER":              "localhost:9999",
		"COLLECTOR_CONFIG":         `{"logLevel":"debug","turnOffScrape":true,"scrapeInterval":2}`,
		"COLLECTION_METHOD":        collectionMethod,
		"MODULE_DOWNLOAD_BASE_URL": "https://collector-modules.stackrox.io/612dd2ee06b660e728292de9393e18c81a88f347ec52a39207c5166b5302b656",
	}
	mounts := map[string]string{
		"/host/var/run/docker.sock:ro": "/var/run/docker.sock",
		"/host/proc:ro":                "/proc",
		"/host/etc:ro":                 "/etc/",
		"/host/usr/lib:ro":             "/usr/lib/",
		"/host/sys:ro":                 "/sys/",
		"/host/dev:ro":                 "/dev",
	}

	collectorImage := ReadEnvVar("COLLECTOR_IMAGE")

	return &collectorManager{
		DBPath:            "/tmp/collector-test.db",
		executor:          e,
		DisableGrpcServer: false,
		BootstrapOnly:     false,
		CollectorImage:    collectorImage,
		GRPCServerImage:   "stackrox/grpc-server:3.0.41.x-159-ge581685bb9",
		Env:               env,
		Mounts:            mounts,
		TestName:          name,
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
	if !c.DisableGrpcServer {
		if _, err := c.executor.CopyFromHost(c.DBPath, c.DBPath); err != nil {
			return err
		}
		c.abortContainer("grpc-server")
		c.captureLogs("grpc-server")
		c.killContainer("grpc-server")
	}

	c.abortContainer("collector")
	c.captureLogs("collector")
	c.killContainer("collector")
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

func (c *collectorManager) launchGRPCServer() error {
	user, _ := user.Current()
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
	cmd := []string{"docker", "run",
		"--name", "collector",
		"--privileged",
		"--network=host"}

	if !c.BootstrapOnly {
		cmd = append(cmd, "-d")
	}

	for dst, src := range c.Mounts {
		cmd = append(cmd, "-v", src+":"+dst)
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

func (c *collectorManager) captureLogs(containerName string) error {
	var lastErr error
	for action, extension := range map[string]string{
		"logs":    "log",
		"inspect": "inspect",
	} {
		logs, err := c.executor.Exec("docker", action, containerName)
		if err != nil {
			fmt.Printf("docker %s error (%v) for container %s\n", action, err, containerName)
			lastErr = err
			continue
		}

		logDirectory := filepath.Join(".", "container-logs")
		os.MkdirAll(logDirectory, os.ModePerm)
		logFile := filepath.Join(logDirectory, c.TestName+"-"+containerName+"."+extension)
		err = ioutil.WriteFile(logFile, []byte(logs), 0644)
		if err != nil {
			lastErr = err
			continue
		}
	}
	return lastErr
}

func (c *collectorManager) abortContainer(name string) error {
	_, err := c.executor.Exec("docker", "kill", "-s", "ABRT", name)
	return err
}

func (c *collectorManager) killContainer(name string) error {
	_, err := c.executor.Exec("docker", "kill", name)
	if err != nil {
		return err
	}
	_, err = c.executor.Exec("docker", "rm", "-fv", name)
	return err
}
