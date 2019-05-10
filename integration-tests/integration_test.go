package integrationtests

import (
	"errors"
	"fmt"
	"os/user"
	"strings"
	"testing"
	"time"

	"encoding/json"
	"io/ioutil"

	"github.com/boltdb/bolt"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/stretchr/testify/suite"
)

const (
	processBucket = "Process"
	networkBucket = "Network"
)

func TestCollectorGRPC(t *testing.T) {
	suite.Run(t, new(IntegrationTestSuite))
}

//func TestCollectorBootstrap(t *testing.T) {
//	suite.Run(t, new(BootstrapTestSuite))
//}

type collectorManager struct {
	Mounts           map[string]string
	Env              map[string]string
	executor         Executor
	dbpath           string
	collectorTag     string
	collectionMethod string
}

func NewCollectorManager(e Executor) *collectorManager {
	return &collectorManager{executor: e}
}

func (c *collectorManager) Setup() error {
	c.dbpath = "/tmp/collector-test.db"
	c.collectorTag = ReadEnvVar("COLLECTOR_TAG")
	c.collectionMethod = ReadEnvVarWithDefault("COLLECTION_METHOD", "kernel_module")
	if strings.Contains(c.collectionMethod, "module") {
		c.collectionMethod = "kernel_module"
	}

	err := c.executor.PullImage("stackrox/collector:" + c.collectorTag)
	if err != nil {
		return err
	}

	err = c.executor.PullImage("stackrox/grpc-server:2.3.16.0-99-g0b961f9515")
	if err != nil {
		return err
	}

	// remove previous db file
	_, err = c.executor.Exec("rm", "-fv", c.dbpath)
	return err
}

func (c *collectorManager) SetupDefaultMounts() {
	c.Mounts = map[string]string{
		"/var/run/docker.sock": "/host/var/run/docker.sock:ro",
		"/proc":                "/host/proc:ro",
		"/etc/":                "/host/etc:ro",
		"/usr/lib/":            "/host/usr/lib:ro",
		"/sys/":                "/host/sys:ro",
		"/dev":                 "/host/dev:ro",
	}
}

func (c *collectorManager) SetupDefaultEnv() {
	c.Env = map[string]string{
		"GRPC_SERVER":       "localhost:9999",
		"COLLECTOR_CONFIG":  `{"logLevel":"debug","turnOffScrape":true,"scrapeInterval":2}`,
		"COLLECTION_METHOD": c.collectionMethod,
	}
}

func (c *collectorManager) Launch() error {
	err := c.launchGRPCServer()
	if err != nil {
		return err
	}
	return c.launchCollector()
}

func (c *collectorManager) TearDown() error {
	c.CaptureLogs("collector", "collector.logs")
	c.CaptureLogs("grpc-server", "grpc-server.logs")
	c.killContainer("grpc-server")

	c.killContainer("collector")
	_, err := c.executor.CopyFromHost(c.dbpath, c.dbpath)
	return err
}

func (c *collectorManager) BoltDB() (db *bolt.DB, err error) {
	opts := &bolt.Options{ReadOnly: true}
	db, err = bolt.Open(c.dbpath, 0600, opts)
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
		"stackrox/grpc-server:2.3.16.0-99-g0b961f9515"}
	_, err := c.executor.Exec(cmd...)
	return err
}

func (c *collectorManager) launchCollector() error {
	cmd := []string{"docker", "run",
		"-d",
		"--rm",
		"--name", "collector",
		"--privileged",
		"--network=host"}
	img := "stackrox/collector:" + c.collectorTag
	for k, v := range c.Mounts {
		cmd = append(cmd, "-v", k+":"+v)
	}
	for k, v := range c.Env {
		cmd = append(cmd, "--env", k+"="+v)
	}

	cmd = append(cmd, img)

	containerID, err := c.executor.Exec(cmd...)
	if err != nil {
		return err
	}

	cid := containerID[0:12]
	running, err := c.executor.Exec("docker", "inspect", "-f", "'{{.State.Running}}'", cid)
	if err == nil && strings.Trim(running, "'\"\n") != "true" {
		return errors.New("collector is not running")
	}
	return err
}

func (c *collectorManager) CaptureLogs(containerName, logFile string) (string, error) {
	logs, err := c.executor.Exec("docker", "logs", containerName)
	if err != nil {
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
	_, err = c.executor.Exec("docker", "rm", name)
	return err
}

type BootstrapTestSuite struct {
	suite.Suite
	executor Executor
}

func (s *BootstrapTestSuite) SetupSuite() {
	s.executor = NewExecutor()
	collector := NewCollectorManager(s.executor)

	err := collector.Setup()
	require.NoError(s.T(), err)

	collector.SetupDefaultEnv()
	collector.SetupDefaultMounts()
	collector.Env["KERNEL_VERSION"] = "3.10.0-514.10.2.el7.x86_64"
	collector.Env["COLLECTION_METHOD"] = "ebpf"

	err = collector.Launch()
	require.NoError(s.T(), err)

	collectorLogs, err := collector.CaptureLogs("collector", "bootstrap_collector.logs")
	fmt.Printf("collector logs:\n%s\n", collectorLogs)

	//err = collector.TearDown()
	//require.NoError(s.T(), err)

	//ff := []byte("temporary file's content")

	//tmpfile, err := ioutil.TempFile("", "example")
	//if err != nil {
	//	log.Fatal(err)
	//}

	//defer os.Remove(tmpfile.Name()) // clean up

	//if _, err := tmpfile.Write(content); err != nil {
	//	log.Fatal(err)
	//}
	//if err := tmpfile.Close(); err != nil {
	//	log.Fatal(err)
	//}
}

type IntegrationTestSuite struct {
	suite.Suite
	db              *bolt.DB
	executor        Executor
	clientContainer string
	clientIP        string
	clientPort      string
	serverContainer string
	serverIP        string
	serverPort      string
}

// Launches collector
// Launches gRPC server in insecure mode
// Launches nginx container
// Execs into nginx and does a sleep
func (s *IntegrationTestSuite) SetupSuite() {

	s.executor = NewExecutor()
	collector := NewCollectorManager(s.executor)

	err := collector.Setup()
	require.NoError(s.T(), err)

	collector.SetupDefaultEnv()
	collector.SetupDefaultMounts()

	err = collector.Launch()
	require.NoError(s.T(), err)

	images := []string{
		"nginx:1.14-alpine",
		"pstauffer/curl:latest",
	}

	for _, image := range images {
		err := s.executor.PullImage(image)
		require.NoError(s.T(), err)
	}

	time.Sleep(10 * time.Second)

	// invokes default nginx
	containerID, err := s.launchContainer("nginx", "nginx:1.14-alpine")
	require.NoError(s.T(), err)
	s.serverContainer = containerID[0:12]

	// invokes "sleep" and "sh" and "ls"
	_, err = s.execContainer("nginx", []string{"sleep", "5"})
	require.NoError(s.T(), err)
	_, err = s.execContainer("nginx", []string{"sh", "-c", "ls"})
	require.NoError(s.T(), err)

	//// start performance container
	//err := s.executor.PullImage("ljishen/sysbench")
	//s.NoError(err)
	//_, err = s.launchPerformanceContainer()
	//s.NoError(err)

	// invokes another container
	containerID, err = s.launchContainer("nginx-curl", "pstauffer/curl:latest", "sleep", "300")
	require.NoError(s.T(), err)
	s.clientContainer = containerID[0:12]

	ip, err := s.getIPAddress("nginx")
	require.NoError(s.T(), err)
	s.serverIP = ip

	port, err := s.getPort("nginx")
	require.NoError(s.T(), err)

	s.serverPort = port

	_, err = s.execContainer("nginx-curl", []string{"curl", s.serverIP})
	require.NoError(s.T(), err)

	ip, err = s.getIPAddress("nginx-curl")
	require.NoError(s.T(), err)
	s.clientIP = ip

	time.Sleep(10 * time.Second)

	err = collector.TearDown()
	require.NoError(s.T(), err)

	s.db, err = collector.BoltDB()
	require.NoError(s.T(), err)
}

func (s *IntegrationTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"nginx", "nginx-curl", "collector"})
}

func (s *IntegrationTestSuite) TestProcessViz() {
	processName := "nginx"
	exeFilePath := "/usr/sbin/nginx"
	expectedProcessInfo := fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err := s.Get(processName, processBucket)
	require.NoError(s.T(), err)
	assert.Equal(s.T(), expectedProcessInfo, val)

	processName = "sh"
	exeFilePath = "/bin/sh"
	expectedProcessInfo = fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err = s.Get(processName, processBucket)
	require.NoError(s.T(), err)
	assert.Equal(s.T(), expectedProcessInfo, val)

	processName = "sleep"
	exeFilePath = "/bin/sleep"
	expectedProcessInfo = fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err = s.Get(processName, processBucket)
	require.NoError(s.T(), err)
	assert.Equal(s.T(), expectedProcessInfo, val)
}

func (s *IntegrationTestSuite) TestNetworkFlows() {

	// Server side checks
	val, err := s.Get(s.serverContainer, networkBucket)
	require.NoError(s.T(), err)
	actualValues := strings.Split(string(val), ":")

	if len(actualValues) < 3 {
		assert.FailNow(s.T(), "serverContainer networkBucket was missing data. ", "val=\"%s\"", val)
	}
	actualServerIP := actualValues[0]
	actualServerPort := actualValues[1]
	actualClientIP := actualValues[2]
	// client port are chosen at random so not checking that

	assert.Equal(s.T(), s.serverIP, actualServerIP)
	assert.Equal(s.T(), s.serverPort, actualServerPort)
	assert.Equal(s.T(), s.clientIP, actualClientIP)

	fmt.Printf("ServerDetails from Bolt: %s %s\n", s.serverContainer, string(val))
	fmt.Printf("ServerDetails from test: %s %s, Port: %s\n", s.serverContainer, s.serverIP, s.serverPort)

	// client side checks
	val, err = s.Get(s.clientContainer, networkBucket)
	actualValues = strings.Split(string(val), ":")
	require.NoError(s.T(), err)

	actualClientIP = actualValues[0]
	actualServerIP = actualValues[2]
	actualServerPort = actualValues[3]

	assert.Equal(s.T(), s.clientIP, actualClientIP)
	assert.Equal(s.T(), s.serverIP, actualServerIP)
	assert.Equal(s.T(), s.serverPort, actualServerPort)

	fmt.Printf("ClientDetails from Bolt: %s %s\n", s.clientContainer, string(val))
	fmt.Printf("ClientDetails from test: %s %s, Port: %s\n", s.clientContainer, s.clientIP, s.clientPort)
}

func (s *IntegrationTestSuite) launchContainer(args ...string) (string, error) {
	cmd := []string{"docker", "run", "-d", "--name"}
	cmd = append(cmd, args...)
	output, err := s.executor.Exec(cmd...)
	outLines := strings.Split(output, "\n")
	return outLines[len(outLines)-1], err
}

func (s *IntegrationTestSuite) execContainer(containerName string, command []string) (string, error) {
	cmd := []string{"docker", "exec", containerName}
	cmd = append(cmd, command...)
	return s.executor.Exec(cmd...)
}

func (s *IntegrationTestSuite) cleanupContainer(containers []string) {
	for _, container := range containers {
		s.executor.Exec("docker", "kill", container)
		s.executor.Exec("docker", "rm", container)
	}
}

func (s *IntegrationTestSuite) containerLogs(containerName string) (string, error) {
	return s.executor.Exec("docker", "logs", containerName)
}

func (s *IntegrationTestSuite) getIPAddress(containerName string) (string, error) {
	stdoutStderr, err := s.executor.Exec("docker", "inspect", "--format='{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'", containerName)
	return strings.Replace(string(stdoutStderr), "'", "", -1), err
}

func (s *IntegrationTestSuite) getPort(containerName string) (string, error) {
	stdoutStderr, err := s.executor.Exec("docker", "inspect", "--format='{{json .NetworkSettings.Ports}}'", containerName)
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

func (s *IntegrationTestSuite) Get(key string, bucket string) (val string, err error) {
	if s.db == nil {
		return "", fmt.Errorf("Db %v is nil", s.db)
	}
	err = s.db.View(func(tx *bolt.Tx) error {
		b := tx.Bucket([]byte(bucket))
		if b == nil {
			return fmt.Errorf("Bucket %s was not found", bucket)
		}
		val = string(b.Get([]byte(key)))
		return nil
	})
	return
}
