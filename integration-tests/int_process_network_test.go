package integrationtests

import (
	"fmt"
	"time"
	"strings"
	"testing"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/suite"
)

type ProcessNetworkTestSuite struct {
	IntegrationTestSuiteBase
	clientContainer string
	clientIP        string
	serverContainer string
	serverIP        string
	serverPort      string
}

func TestProcessNetwork(t *testing.T) {
	suite.Run(t, new(ProcessNetworkTestSuite))
}

// Launches collector
// Launches gRPC server in insecure mode
// Launches nginx container
// Execs into nginx and does a sleep
func (s *ProcessNetworkTestSuite) SetupSuite() {

	s.metrics = map[string]float64{}
	s.executor = NewExecutor()
	s.docker = NewDocker(s.executor)
	s.StartContainerStats()
	s.collector = NewCollectorManager(s.executor, s.T().Name())

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)

	images := []string{
		"nginx:1.14-alpine",
		"pstauffer/curl:latest",
	}

	for _, image := range images {
		err := s.docker.Pull(image)
		s.Require().NoError(err)
	}

	time.Sleep(10 * time.Second)

	// invokes default nginx
	containerID, err := s.launchContainer("nginx", "nginx:1.14-alpine")
	s.Require().NoError(err)
	s.serverContainer = containerShortID(containerID)

	// invokes "sleep" and "sh" and "ls"
	_, err = s.execContainer("nginx", []string{"sleep", "5"})
	s.Require().NoError(err)
	_, err = s.execContainer("nginx", []string{"sh", "-c", "ls"})
	s.Require().NoError(err)

	// invokes another container
	containerID, err = s.launchContainer("nginx-curl", "pstauffer/curl:latest", "sleep", "300")
	s.Require().NoError(err)
	s.clientContainer = containerShortID(containerID)

	s.serverIP, err = s.getIPAddress("nginx")
	s.Require().NoError(err)

	s.serverPort, err = s.getPort("nginx")
	s.Require().NoError(err)

	_, err = s.execContainer("nginx-curl", []string{"curl", fmt.Sprintf("%s:%s", s.serverIP, s.serverPort)})
	s.Require().NoError(err)

	s.clientIP, err = s.getIPAddress("nginx-curl")
	s.Require().NoError(err)

	time.Sleep(10 * time.Second)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *ProcessNetworkTestSuite) TearDownSuite() {
	s.cleanupContainers("nginx", "nginx-curl")
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("process_network", stats, s.metrics)
}

func (s *ProcessNetworkTestSuite) TestProcessViz() {
	processName := "nginx"
	exeFilePath := "/usr/sbin/nginx"
	expectedProcessInfo := fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err := s.Get(processName, processBucket)
	s.Require().NoError(err)
	assert.Equal(s.T(), expectedProcessInfo, val)

	processName = "sh"
	exeFilePath = "/bin/sh"
	expectedProcessInfo = fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err = s.Get(processName, processBucket)
	s.Require().NoError(err)
	assert.Equal(s.T(), expectedProcessInfo, val)

	processName = "sleep"
	exeFilePath = "/bin/sleep"
	expectedProcessInfo = fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err = s.Get(processName, processBucket)
	s.Require().NoError(err)
	assert.Equal(s.T(), expectedProcessInfo, val)
}

func (s *ProcessNetworkTestSuite) TestProcessLineageInfo() {
	processName := "awk"
	exeFilePath := "/usr/bin/awk"
	parentFilePath := "/bin/busybox"
	expectedProcessLineageInfo := fmt.Sprintf("%s:%s:%s:%d:%s:%s", processName, exeFilePath, parentUIDStr, 0, parentExecFilePathStr, parentFilePath)
	val, err := s.GetLineageInfo(processName, "0", processLineageInfoBucket)
	s.Require().NoError(err)
	assert.Equal(s.T(), expectedProcessLineageInfo, val)

	processName = "grep"
	exeFilePath = "/bin/grep"
	parentFilePath = "/bin/busybox"
	expectedProcessLineageInfo = fmt.Sprintf("%s:%s:%s:%d:%s:%s", processName, exeFilePath, parentUIDStr, 0, parentExecFilePathStr, parentFilePath)
	val, err = s.GetLineageInfo(processName, "0", processLineageInfoBucket)
	s.Require().NoError(err)
	assert.Equal(s.T(), expectedProcessLineageInfo, val)

	processName = "sleep"
	exeFilePath = "/bin/sleep"
	parentFilePath = "/bin/busybox"
	expectedProcessLineageInfo = fmt.Sprintf("%s:%s:%s:%d:%s:%s", processName, exeFilePath, parentUIDStr, 0, parentExecFilePathStr, parentFilePath)
	val, err = s.GetLineageInfo(processName, "0", processLineageInfoBucket)
	s.Require().NoError(err)
	assert.Equal(s.T(), expectedProcessLineageInfo, val)
}

func (s *ProcessNetworkTestSuite) TestNetworkFlows() {

	// Server side checks
	val, err := s.Get(s.serverContainer, networkBucket)
	s.Require().NoError(err)
	actualValues := strings.Split(string(val), "|")

	if len(actualValues) < 2 {
		assert.FailNow(s.T(), "serverContainer networkBucket was missing data. ", "val=\"%s\"", val)
	}
	actualServerEndpoint := actualValues[0]
	actualClientEndpoint := actualValues[1]

	// From server perspective, network connection info only has local port and remote IP
	assert.Equal(s.T(), fmt.Sprintf(":%s", s.serverPort), actualServerEndpoint)
	assert.Equal(s.T(), s.clientIP, actualClientEndpoint)

	fmt.Printf("ServerDetails from Bolt: %s %s\n", s.serverContainer, string(val))
	fmt.Printf("ServerDetails from test: %s %s, Port: %s\n", s.serverContainer, s.serverIP, s.serverPort)

	// client side checks
	val, err = s.Get(s.clientContainer, networkBucket)
	s.Require().NoError(err)
	actualValues = strings.Split(string(val), "|")

	actualClientEndpoint = actualValues[0]
	actualServerEndpoint = actualValues[1]

	// From client perspective, network connection info has no local endpoint and full remote endpoint
	assert.Empty(s.T(), actualClientEndpoint)
	assert.Equal(s.T(), fmt.Sprintf("%s:%s", s.serverIP, s.serverPort), actualServerEndpoint)

	fmt.Printf("ClientDetails from Bolt: %s %s\n", s.clientContainer, string(val))
	fmt.Printf("ClientDetails from test: %s %s\n", s.clientContainer, s.clientIP)
}