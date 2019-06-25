package integrationtests

import (
	"fmt"
	"io/ioutil"
	"os/user"
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
}

func NewCollectorManager(e Executor) *collectorManager {
	collectionMethod := ReadEnvVarWithDefault("COLLECTION_METHOD", "kernel_module")
	if strings.Contains(collectionMethod, "module") {
		collectionMethod = "kernel_module"
	}

	env := map[string]string{
		"GRPC_SERVER":       "localhost:9999",
		"COLLECTOR_CONFIG":  `{"logLevel":"debug","turnOffScrape":true,"scrapeInterval":2}`,
		"COLLECTION_METHOD": collectionMethod,
	}
	mounts := map[string]string{
		"/host/var/run/docker.sock:ro": "/var/run/docker.sock",
		"/host/proc:ro":                "/proc",
		"/host/etc:ro":                 "/etc/",
		"/host/usr/lib:ro":             "/usr/lib/",
		"/host/sys:ro":                 "/sys/",
		"/host/dev:ro":                 "/dev",
	}

	collectorTag := ReadEnvVar("COLLECTOR_TAG")

	return &collectorManager{
		DBPath:            "/tmp/collector-test.db",
		executor:          e,
		DisableGrpcServer: false,
		BootstrapOnly:     false,
		CollectorImage:    "stackrox/collector:" + collectorTag,
		GRPCServerImage:   "stackrox/grpc-server:2.3.16.0-99-g0b961f9515",
		Env:               env,
		Mounts:            mounts,
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
		c.captureLogs("grpc-server", "grpc-server.logs")
		c.killContainer("grpc-server")
	}

	c.captureLogs("collector", "collector.logs")
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

func (c *collectorManager) captureLogs(containerName, logFile string) (string, error) {
	logs, err := c.executor.Exec("docker", "logs", containerName)
	if err != nil {
		fmt.Printf("docker logs error (%v) for container %s: %s\n", err, containerName, logFile)
		return "", err
	}
	err = ioutil.WriteFile(logFile, []byte(logs), 0644)
	if err != nil {
		return "", err
	}
	return logs, nil
}

func (c *collectorManager) killContainer(name string) error {
	_, err := c.executor.Exec("docker", "kill", name)
	if err != nil {
		return err
	}
	_, err = c.executor.Exec("docker", "rm", "-fv", name)
	return err
}
