package integrationtests

import (
	"fmt"
	"strings"
	"testing"
	"time"

	"encoding/json"

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
