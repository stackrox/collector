package integrationtests

import (
	"bytes"
	"fmt"
	"os"
	"os/exec"
	"strings"
	"testing"
	"time"

	"github.com/stretchr/testify/suite"
	"github.com/stretchr/testify/assert"
	"github.com/boltdb/bolt"
	"io/ioutil"
	"encoding/json"
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
	dbpath      string
	db          *bolt.DB
	containerID string
	ipAddress   string
	port        string
}

// Launches collector
// Launches gRPC server in insecure mode
// Launches nginx container
// Execs into nginx and does a sleep
func (s *IntegrationTestSuite) SetupSuite() {
	s.dockerComposeUp()
	s.dbpath = "/tmp/collector-test.db"

	// invokes default nginx
	_, err := s.launchContainer("nginx", "nginx:1.14-alpine", "0:0")
	assert.Nil(s.T(), err)

	// invokes "sleep"
	_, err = s.execContainer("nginx", []string{"sh", "-c", "sleep 5"}, "0:0")
	assert.Nil(s.T(), err)

	// invokes another container
	containerID, err := s.launchContainer("busybox", "busybox:1", "666:666")
	assert.Nil(s.T(), err)
	s.containerID = containerID[0:12]

	ip, err := s.getIPAddress("nginx")
	assert.Nil(s.T(), err)
	s.ipAddress = ip

	port, err := s.getPort("nginx")
	assert.Nil(s.T(), err)
	s.port = port

	// invokes "wget ip from busybox"
	_, err = s.execContainer("busybox", []string{"wget", ip}, "666:666")
	assert.Nil(s.T(), err)

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
	exeFilePath := "/usr/sbin/nginx"
	expectedProcessInfo := fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err := s.Get(processName, processBucket)
	assert.Nil(s.T(), err)
	assert.Equal(s.T(), expectedProcessInfo, val)

	processName = "sh"
	exeFilePath = "/bin/sh"
	expectedProcessInfo = fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 666, 666)
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
	val, err := s.Get(s.containerID, networkBucket)
	assert.Nil(s.T(), err)
	fmt.Printf("from db: %s\n", string(val))
	fmt.Printf("from test IP: %s, Port: %s\n", s.ipAddress, s.port)
}

func (s *IntegrationTestSuite) TearDownSuite() {
	s.dockerComposeDown()
	s.cleanupContainer([]string{"foo"})
}

func (s *IntegrationTestSuite) launchContainer(containerName, imageName, id string) (string, error) {
	cmd := exec.Command("docker", "run", "-u", id, "-d", "--name", containerName, imageName)
	stdoutStderr, err := cmd.CombinedOutput()
	return strings.Trim(string(stdoutStderr), "\n"), err
}

func (s *IntegrationTestSuite) execContainer(containerName string, command []string, id string) (string, error) {
	args := fmt.Sprintf("exec -u %s %s %s", id, containerName, strings.Join(command, " "))
	cmd := exec.Command("docker", args)
	stdoutStderr, err := cmd.CombinedOutput()
	return strings.Trim(string(stdoutStderr), "\n"), err
}

func (s *IntegrationTestSuite) getIPAddress(containerName string) (string, error) {
	cmd := exec.Command("docker", "inspect", "--format='{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'", containerName)
	stdoutStderr, err := cmd.CombinedOutput()
	return strings.Trim(string(stdoutStderr), "\n"), err
}

func (s *IntegrationTestSuite) getPort(containerName string) (string, error) {
	cmd := exec.Command("docker", "inspect", "--format='--format='{{json .NetworkSettings.Ports}}''", containerName)
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
	err := s.dockerCompose("docker-compose", "--project-name", "test", "--file", "docker-compose.yml", "up", "-d")
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
	err := s.dockerCompose("docker-compose", "--project-name", "test", "--file", "docker-compose.yml", "down", "--volumes")
	if err != nil {
		s.T().Fatal("Unable to tear down the stack", err.Error())
	}

	return err
}

func (s *IntegrationTestSuite) dockerComposePorts(serviceName, containerPort string) (*bytes.Buffer, error) {
	return s.dockerComposeStdout("docker-compose", "--project-name", "test", "--file", "docker-compose.yml", "port", serviceName, containerPort)
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
