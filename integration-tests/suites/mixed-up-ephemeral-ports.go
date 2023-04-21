package suites

import (
	"fmt"
	"strings"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stretchr/testify/assert"
)

type Container struct {
	Name             string
	Cmd              string
	ContainerID      string
	ListenIP         string
	ExpectedNetwork  string
	ExpectedEndpoint []common.EndpointInfo
}

type ServerClientPair struct {
	server Container
	client Container
}

type MixedUpEphemeralPortsTestSuite struct {
	IntegrationTestSuiteBase
	Server Container
	Client Container
}

func (s *MixedUpEphemeralPortsTestSuite) SetupSuite() {

	s.metrics = map[string]float64{}
	s.executor = common.NewExecutor()
	s.StartContainerStats()
	s.collector = common.NewCollectorManager(s.executor, s.T().Name())

	s.collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":false,"scrapeInterval":2}`
	s.collector.Env["ROX_PROCESSES_LISTENING_ON_PORT"] = "false"
	s.collector.Env["ROX_ENABLE_AFTERGLOW"] = "0"

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)
	time.Sleep(30 * time.Second)

	socatImage := common.QaImage("quay.io/rhacs-eng/qa", "socat")

	serverName := s.Server.Name
	clientName := s.Client.Name
	longContainerID, err := s.launchContainer(serverName, socatImage, "/bin/sh", "-c", "/bin/sleep 300")
	s.Server.ContainerID = common.ContainerShortID(longContainerID)
	s.Require().NoError(err)
	longContainerID, err = s.launchContainer(clientName, socatImage, "/bin/sh", "-c", "/bin/sleep 300")
	s.Client.ContainerID = common.ContainerShortID(longContainerID)
	_, err = s.execContainer(serverName, []string{"/bin/sh", "-c", s.Server.Cmd})
	s.Require().NoError(err)
	time.Sleep(3 * time.Second)
	s.Server.ListenIP, err = s.getIPAddress(serverName)
	s.Require().NoError(err)
	clientCmd := strings.Replace(s.Client.Cmd, "LISTEN_IP", s.Server.ListenIP, -1)
	_, err = s.execContainer(clientName, []string{"/bin/sh", "-c", clientCmd})
	s.Require().NoError(err)
	time.Sleep(6 * time.Second)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *MixedUpEphemeralPortsTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"collector"})
	s.cleanupContainer([]string{s.Server.Name})
	s.cleanupContainer([]string{s.Client.Name})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("EphemeralPorts", stats, s.metrics)
}

func (s *MixedUpEphemeralPortsTestSuite) TestMixedUpEphemeralPorts() {
	fmt.Println()
	fmt.Println("serverName= ", s.Server.Name)
	fmt.Println("clientName= ", s.Client.Name)
	fmt.Println("serverCmd= ", s.Server.Cmd)
	fmt.Println("clientCmd= ", s.Client.Cmd)

	val, err := s.Get(s.Client.ContainerID, networkBucket)
	s.Require().NoError(err)
	actualValues := strings.Split(string(val), "|")
	assert.Equal(s.T(), "ROLE_CLIENT", actualValues[2])

	val, err = s.Get(s.Server.ContainerID, networkBucket)
	s.Require().NoError(err)
	actualValues = strings.Split(string(val), "|")
	assert.Equal(s.T(), "ROLE_SERVER", actualValues[2])
}
