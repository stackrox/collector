package integrationtests

import (
	"fmt"
	"strings"
	"testing"
	"time"

	"encoding/json"
	"io/ioutil"

	"github.com/boltdb/bolt"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/suite"
)

const (
	processBucket = "Process"
	networkBucket = "Network"
)

func TestCollectorGRPC(t *testing.T) {
	suite.Run(t, new(IntegrationTestSuite))
}

type IntegrationTestSuite struct {
	suite.Suite
	db               *bolt.DB
	executor         Executor
	clientContainer  string
	clientIP         string
	clientPort       string
	collectionMethod string
	collectorTag     string
	dbpath           string
	serverContainer  string
	serverIP         string
	serverPort       string
}

// Launches collector
// Launches gRPC server in insecure mode
// Launches nginx container
// Execs into nginx and does a sleep
func (s *IntegrationTestSuite) SetupSuite() {

	s.dbpath = "/tmp/collector-test.db"
	s.collectorTag = ReadEnvVar("COLLECTOR_TAG")

	s.collectionMethod = ReadEnvVarWithDefault("COLLECTION_METHOD", "kernel_module")
	if strings.Contains(s.collectionMethod, "module") {
		s.collectionMethod = "kernel_module"
	}

	s.executor = NewExecutor()

	images := []string{
		"stackrox/collector:" + s.collectorTag,
		"nginx:1.14-alpine",
		"pstauffer/curl:latest",
		"stackrox/grpc-server:2.3.16.0-99-g0b961f9515"}

	for _, image := range images {
		s.executor.PullImage(image)
	}

	s.resetDBFile(s.dbpath)

	containerID, err := s.launchGRPCServer()
	s.NoError(err)

	containerID, err = s.launchCollector()
	s.NoError(err)
	collectorContainer := containerID

	running, err := s.executor.Exec("docker", "inspect", "-f", "'{{.State.Running}}'", collectorContainer)
	if strings.Trim(running, "'\"\n") != "true" {
		assert.FailNow(s.T(), "collecter not running")
	}

	time.Sleep(10 * time.Second)

	// invokes default nginx
	containerID, err = s.launchContainer("nginx", "nginx:1.14-alpine")
	s.NoError(err)
	s.serverContainer = containerID[0:12]

	// invokes "sleep" and "sh" and "ls"
	_, err = s.execContainer("nginx", []string{"sleep", "5"})
	s.NoError(err)
	_, err = s.execContainer("nginx", []string{"sh", "-c", "ls"})
	s.NoError(err)

	//// start performance container
	//err := s.executor.PullImage("ljishen/sysbench")
	//s.NoError(err)
	//_, err = s.launchPerformanceContainer()
	//s.NoError(err)

	// invokes another container
	containerID, err = s.launchContainer("nginx-curl", "pstauffer/curl:latest", "sleep", "300")
	s.NoError(err)
	s.clientContainer = containerID[0:12]

	ip, err := s.getIPAddress("nginx")
	s.NoError(err)
	s.serverIP = ip

	port, err := s.getPort("nginx")
	s.NoError(err)
	s.serverPort = port

	_, err = s.execContainer("nginx-curl", []string{"curl", s.serverIP})
	s.NoError(err)

	ip, err = s.getIPAddress("nginx-curl")
	s.NoError(err)
	s.clientIP = ip

	time.Sleep(10 * time.Second)

	_, err = s.copyDBFile(s.dbpath)
	s.NoError(err)

	logs, err := s.containerLogs("collector")
	s.NoError(err)
	err = ioutil.WriteFile("collector.logs", []byte(logs), 0644)
	s.NoError(err)

	logs, err = s.containerLogs("grpc-server")
	err = ioutil.WriteFile("grpc_server.logs", []byte(logs), 0644)
	s.NoError(err)

	s.cleanupContainer([]string{"grpc-server"})

	s.db, err = s.BoltDB()
	if err != nil {
		assert.FailNow(s.T(), "DB file could not be opened", "Error: %s", err)
	}
}

func (s *IntegrationTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"nginx", "nginx-curl", "collector"})
}

func (s *IntegrationTestSuite) TestProcessViz() {
	processName := "nginx"
	exeFilePath := "/usr/sbin/nginx"
	expectedProcessInfo := fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err := s.Get(processName, processBucket)
	s.NoError(err)
	assert.Equal(s.T(), expectedProcessInfo, val)

	processName = "sh"
	exeFilePath = "/bin/sh"
	expectedProcessInfo = fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err = s.Get(processName, processBucket)
	s.NoError(err)
	assert.Equal(s.T(), expectedProcessInfo, val)

	processName = "sleep"
	exeFilePath = "/bin/sleep"
	expectedProcessInfo = fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err = s.Get(processName, processBucket)
	s.NoError(err)
	assert.Equal(s.T(), expectedProcessInfo, val)
}

func (s *IntegrationTestSuite) TestNetworkFlows() {

	// Server side checks
	val, err := s.Get(s.serverContainer, networkBucket)
	s.NoError(err)
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
	s.NoError(err)

	actualClientIP = actualValues[0]
	actualServerIP = actualValues[2]
	actualServerPort = actualValues[3]

	assert.Equal(s.T(), s.clientIP, actualClientIP)
	assert.Equal(s.T(), s.serverIP, actualServerIP)
	assert.Equal(s.T(), s.serverPort, actualServerPort)

	fmt.Printf("ClientDetails from Bolt: %s %s\n", s.clientContainer, string(val))
	fmt.Printf("ClientDetails from test: %s %s, Port: %s\n", s.clientContainer, s.clientIP, s.clientPort)
}

func (s *IntegrationTestSuite) copyDBFile(path string) (string, error) {
	return s.executor.CopyFromHost(path, path)
}

func (s *IntegrationTestSuite) resetDBFile(path string) (string, error) {
	return s.executor.Exec("rm", "-fv", path)
}

func (s *IntegrationTestSuite) launchPerformanceContainer() (string, error) {
	cmd := []string{"docker", "run", "-d", "--rm",
		"-v", "/tmp:/root/results", "ljishen/sysbench",
		"/root/results/output_cpu.prof", "--test=cpu",
		"--cpu-max-prime=10000", "run"}
	return s.executor.Exec(cmd...)
}

func (s *IntegrationTestSuite) launchGRPCServer() (string, error) {
	cmd := []string{"docker", "run",
		"-d",
		"--rm",
		"--name", "grpc-server",
		"--network=host",
		"-v", "/tmp:/tmp:rw",
		"stackrox/grpc-server:2.3.16.0-99-g0b961f9515"}
	return s.executor.Exec(cmd...)
}

func (s *IntegrationTestSuite) launchCollector() (string, error) {
	cmd := []string{"docker", "run",
		"-d",
		"--rm",
		"--name", "collector",
		"--privileged",
		"--network=host"}

	mounts := []string{
		"-v", "/var/run/docker.sock:/host/var/run/docker.sock:ro",
		"-v", "/proc:/host/proc:ro",
		"-v", "/etc/:/host/etc:ro",
		"-v", "/usr/lib/:/host/usr/lib:ro",
		"-v", "/sys/:/host/sys:ro",
		"-v", "/dev:/host/dev:ro",
	}

	env := []string{
		"--env", "GRPC_SERVER=localhost:9999",
		"--env", `COLLECTOR_CONFIG={"logLevel":"debug","turnOffScrape":true,"scrapeInterval":2}`,
		"--env", "COLLECTION_METHOD=" + s.collectionMethod}

	img := "stackrox/collector:" + s.collectorTag

	cmd = append(cmd, mounts...)
	cmd = append(cmd, env...)
	cmd = append(cmd, img)
	return s.executor.Exec(cmd...)
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

func (s *IntegrationTestSuite) BoltDB() (db *bolt.DB, err error) {
	opts := &bolt.Options{ReadOnly: true}
	db, err = bolt.Open(s.dbpath, 0600, opts)
	if err != nil {
		fmt.Printf("Permission error. %v\n", err)
	}
	return db, err
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
