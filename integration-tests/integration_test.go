package integrationtests

import (
	"bytes"
	"fmt"
	"os"
	"os/exec"
	"strconv"
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
	dbpath          string
	db              *bolt.DB
	serverIP        string
	serverPort      string
	clientIP        string
	clientPort      string
	serverContainer string
	clientContainer string
	useEbpf         bool
	composeFile     string
}

// Launches collector
// Launches gRPC server in insecure mode
// Launches nginx container
// Execs into nginx and does a sleep
func (s *IntegrationTestSuite) SetupSuite() {

	s.composeFile = "docker-compose.yml"
	s.useEbpf = false

	useEbpfEnvVar := "ROX_COLLECTOR_EBPF"
	if e, ok := os.LookupEnv(useEbpfEnvVar); ok {
		s.useEbpf, _ = strconv.ParseBool(e)
	}

	if s.useEbpf {
		s.composeFile = "docker-compose-ebpf.yml"
	}

	s.dockerComposeUp()
	s.dbpath = "/tmp/collector-test.db"

	// invokes default nginx
	containerID, err := s.launchContainer("nginx", "nginx:1.14-alpine", "")
	assert.Nil(s.T(), err)
	s.serverContainer = containerID[0:12]

	// invokes "sleep"
	_, err = s.execContainer("nginx", []string{"sh", "-c", "sleep 5"})
	assert.Nil(s.T(), err)

	// start performance container
	_, err = s.launchPerformanceContainer()
	assert.Nil(s.T(), err)

	// invokes another container
	containerID, err = s.launchContainer("nginx-curl", "ewoutp/docker-nginx-curl", "")
	assert.Nil(s.T(), err)
	s.clientContainer = containerID[0:12]

	ip, err := s.getIPAddress("nginx")
	assert.Nil(s.T(), err)
	s.serverIP = ip

	port, err := s.getPort("nginx")
	assert.Nil(s.T(), err)
	s.serverPort = port

	_, err = s.execContainer("nginx-curl", []string{"curl", ip})
	assert.Nil(s.T(), err)

	ip, err = s.getIPAddress("nginx-curl")
	assert.Nil(s.T(), err)
	s.clientIP = ip

	port, err = s.getPort("nginx-curl")
	assert.Nil(s.T(), err)
	s.clientPort = port

	time.Sleep(20 * time.Second)

	logs, err := s.containerLogs("test_collector_1")
	assert.NoError(s.T(), err)
	err = ioutil.WriteFile("collector.logs", []byte(logs), 0644)
	assert.NoError(s.T(), err)

	logs, err = s.containerLogs("test_grpc-server_1")
	err = ioutil.WriteFile("grpc_server.logs", []byte(logs), 0644)
	assert.NoError(s.T(), err)

	// bring down server
	s.dockerComposeDown()
	// sleep for few
	time.Sleep(2 * time.Second)

	s.db, err = s.BoltDB()
	if err != nil {
		assert.FailNow(s.T(), "DB file could not be opened", "Error: %s", err)
	}
}

func (s *IntegrationTestSuite) TestProcessViz() {
	processName := "nginx"
	exeFilePath := "/usr/local/sbin/nginx"
	expectedProcessInfo := fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err := s.Get(processName, processBucket)
	assert.Nil(s.T(), err)
	assert.Equal(s.T(), expectedProcessInfo, val)

	processName = "sh"
	exeFilePath = "/bin/sh"
	expectedProcessInfo = fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err = s.Get(processName, processBucket)
	assert.Nil(s.T(), err)
	assert.Equal(s.T(), expectedProcessInfo, val)

	processName = "sleep"
	exeFilePath = "/bin/sleep"
	expectedProcessInfo = fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err = s.Get(processName, processBucket)
	assert.Nil(s.T(), err)
	assert.Equal(s.T(), expectedProcessInfo, val)
}

func (s *IntegrationTestSuite) TestNetworkFlows() {

	// Server side checks
	val, err := s.Get(s.serverContainer, networkBucket)
	assert.Nil(s.T(), err)
	actualValues := strings.Split(string(val), ":")

	if len(actualValues) < 3 {
		assert.FailNow(s.T(), "serverContainer networkBucket was missing data. ", "val=\"%s\"", val)
	}
	expectedServerIP := actualValues[0]
	expectedServerPort := actualValues[1]
	expectedClientIP := actualValues[2]
	// client port are chosen at random so not checking that

	assert.Equal(s.T(), expectedServerIP, expectedServerIP)
	assert.Equal(s.T(), expectedServerPort, expectedServerPort)
	assert.Equal(s.T(), expectedClientIP, expectedClientIP)

	fmt.Printf("ServerDetails from Bolt: %s %s\n", s.serverContainer, string(val))
	fmt.Printf("ServerDetails from test: %s %s, Port: %s\n", s.serverContainer, s.serverIP, s.serverPort)

	// client side checks
	val, err = s.Get(s.clientContainer, networkBucket)
	assert.Nil(s.T(), err)
	expectedClientIP = actualValues[0]
	expectedServerIP = actualValues[2]
	expectedServerPort = actualValues[3]
	assert.Equal(s.T(), expectedServerIP, expectedServerIP)
	assert.Equal(s.T(), expectedServerPort, expectedServerPort)
	assert.Equal(s.T(), expectedClientIP, expectedClientIP)

	fmt.Printf("ClientDetails from Bolt: %s %s\n", s.clientContainer, string(val))
	fmt.Printf("ClientDetails from test: %s %s, Port: %s\n", s.clientContainer, s.clientIP, s.clientPort)
}

func (s *IntegrationTestSuite) TearDownSuite() {
	s.dockerComposeDown()
	s.cleanupContainer([]string{"nginx", "nginx-curl"})
}

func (s *IntegrationTestSuite) launchPerformanceContainer() (string, error) {
	var cmd *exec.Cmd
	cmd = exec.Command("docker", "run", "-d", "--rm",
		"-v", "/tmp:/root/results", "ljishen/sysbench",
		"/root/results/output_cpu.prof", "--test=cpu",
		"--cpu-max-prime=10000", "run")
	stdoutStderr, err := cmd.CombinedOutput()
	trimmed := strings.Trim(string(stdoutStderr), "\n")
	return trimmed, err
}

func (s *IntegrationTestSuite) launchContainer(containerName, imageName, command string) (string, error) {
	var cmd *exec.Cmd
	if command != "" {
		cmd = exec.Command("docker", "run", "-d", "--name", containerName, imageName, "/bin/sleep 300")
	} else {
		cmd = exec.Command("docker", "run", "-d", "--name", containerName, imageName)
	}
	stdoutStderr, err := cmd.CombinedOutput()
	trimmed := strings.Trim(string(stdoutStderr), "\n")
	outLines := strings.Split(trimmed, "\n")
	return outLines[len(outLines)-1], err
}

func (s *IntegrationTestSuite) execContainer(containerName string, command []string) (string, error) {
	args := []string{"exec", containerName}
	args = append(args, command...)
	cmd := exec.Command("docker", args...)
	stdoutStderr, err := cmd.CombinedOutput()
	return strings.Trim(string(stdoutStderr), "\n"), err
}

func (s *IntegrationTestSuite) getIPAddress(containerName string) (string, error) {
	cmd := exec.Command("docker", "inspect", "--format='{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'", containerName)
	stdoutStderr, err := cmd.CombinedOutput()
	return strings.Trim(strings.Replace(string(stdoutStderr), "'", "", -1), "\n"), err
}

func (s *IntegrationTestSuite) getPort(containerName string) (string, error) {
	cmd := exec.Command("docker", "inspect", "--format={{json .NetworkSettings.Ports}}", containerName)
	stdoutStderr, err := cmd.CombinedOutput()
	if err != nil {
		return "", err
	}
	rawString := strings.Trim(string(stdoutStderr), "\n")
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

func (s *IntegrationTestSuite) cleanupContainer(containers []string) {
	for _, container := range containers {
		exec.Command("docker", "kill", container).Run()
		exec.Command("docker", "rm", container).Run()
	}
}

func (s *IntegrationTestSuite) dockerComposeUp() error {
	err := s.dockerCompose("docker-compose", "--project-name", "test", "--file", s.composeFile, "up", "-d")
	if err != nil {
		s.T().Fatal("Unable to deploy the stack", err.Error())
	}

	time.Sleep(10 * time.Second)

	return err
}

func (s *IntegrationTestSuite) containerLogs(containerName string) (string, error) {
	cmd := exec.Command("docker", "logs", containerName)
	stdoutStderr, err := cmd.CombinedOutput()
	return strings.Trim(string(stdoutStderr), "\n"), err
}

func (s *IntegrationTestSuite) dockerComposeDown() error {
	err := s.dockerCompose("docker-compose", "--project-name", "test", "--file", s.composeFile, "down", "--volumes")
	if err != nil {
		s.T().Fatal("Unable to tear down the stack", err.Error())
	}

	return err
}

func (s *IntegrationTestSuite) dockerComposePorts(serviceName, containerPort string) (*bytes.Buffer, error) {
	return s.dockerComposeStdout("docker-compose", "--project-name", "test", "--file", s.composeFile, "port", serviceName, containerPort)
}

func (s *IntegrationTestSuite) dockerCompose(name string, arg ...string) error {
	dockerCompose := exec.Command(name, arg...)
	dockerCompose.Stdout = os.Stdout
	dockerCompose.Stderr = os.Stderr

	err := dockerCompose.Run()

	return err
}

func (s *IntegrationTestSuite) dockerComposeStdout(name string, arg ...string) (*bytes.Buffer, error) {
	var stdout bytes.Buffer

	dockerCompose := exec.Command(name, arg...)
	dockerCompose.Stdout = &stdout
	dockerCompose.Stderr = os.Stderr

	err := dockerCompose.Run()

	return &stdout, err
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
