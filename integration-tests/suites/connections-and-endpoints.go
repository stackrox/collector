package suites

import (
	"fmt"
	"sort"
	"strings"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stretchr/testify/assert"
)

type Container struct {
	Name              string
	Cmd               string
	ContainerID       string
	IP                string
	ExpectedNetwork   []common.NetworkInfo
	ExpectedEndpoints []common.EndpointInfo
}

type ConnectionsAndEndpointsTestSuite struct {
	IntegrationTestSuiteBase
	Server Container
	Client Container
}

func (s *ConnectionsAndEndpointsTestSuite) SetupSuite() {

	s.metrics = map[string]float64{}
	s.executor = common.NewExecutor()
	s.StartContainerStats()
	s.collector = common.NewCollectorManager(s.executor, s.T().Name())

	s.collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":false,"scrapeInterval":2}`
	s.collector.Env["ROX_PROCESSES_LISTENING_ON_PORT"] = "true"
	s.collector.Env["ROX_ENABLE_AFTERGLOW"] = "0"
	s.collector.Env["COLLECTION_METHOD"] = "EBPF"

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
	s.Require().NoError(err)
	s.Client.ContainerID = common.ContainerShortID(longContainerID)

	s.Server.IP, err = s.getIPAddress(serverName)
	s.Require().NoError(err)
	s.Client.IP, err = s.getIPAddress(clientName)
	s.Require().NoError(err)

	serverCmd := strings.Replace(s.Server.Cmd, "CLIENT_IP", s.Client.IP, -1)
	fmt.Println(serverCmd)
	_, err = s.execContainer(serverName, []string{"/bin/sh", "-c", serverCmd})
	s.Require().NoError(err)

	time.Sleep(3 * time.Second)

	clientCmd := strings.Replace(s.Client.Cmd, "SERVER_IP", s.Server.IP, -1)
	fmt.Println(clientCmd)
	_, err = s.execContainer(clientName, []string{"/bin/sh", "-c", clientCmd})
	s.Require().NoError(err)
	time.Sleep(6 * time.Second)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *ConnectionsAndEndpointsTestSuite) TearDownSuite() {
	//s.cleanupContainer([]string{"collector"})
	//s.cleanupContainer([]string{s.Server.Name})
	//s.cleanupContainer([]string{s.Client.Name})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("ConnectionsAndEndpoints", stats, s.metrics)
}

func (s *ConnectionsAndEndpointsTestSuite) TestConnectionsAndEndpoints() {

	val, err := s.Get(s.Client.ContainerID, networkBucket)
	s.Require().NoError(err)
	clientNetwork, err := common.NewNetworkInfo(val)
	s.Require().NoError(err)
	expectedLocalAddress := strings.Replace(s.Client.ExpectedNetwork[0].LocalAddress, "CLIENT_IP", s.Client.IP, -1)
	expectedRemoteAddress := strings.Replace(s.Client.ExpectedNetwork[0].RemoteAddress, "SERVER_IP", s.Server.IP, -1)
	assert.Equal(s.T(), expectedLocalAddress, clientNetwork.LocalAddress)
	assert.Equal(s.T(), expectedRemoteAddress, clientNetwork.RemoteAddress)
	assert.Equal(s.T(), "ROLE_CLIENT", clientNetwork.Role)
	assert.Equal(s.T(), s.Client.ExpectedNetwork[0].SocketFamily, clientNetwork.SocketFamily)

	if s.Client.ExpectedEndpoints != nil {
		fmt.Println("Expected client endpoint should be nil")
	}
	_, err = s.GetEndpoints(s.Client.ContainerID)
	s.Require().Error(err, "There should be no client endpoint")

	val, err = s.Get(s.Server.ContainerID, networkBucket)
	s.Require().NoError(err)
	serverNetwork, err := common.NewNetworkInfo(val)
	s.Require().NoError(err)
	expectedLocalAddress = strings.Replace(s.Server.ExpectedNetwork[0].LocalAddress, "SERVER_IP", s.Server.IP, -1)
	expectedRemoteAddress = strings.Replace(s.Server.ExpectedNetwork[0].RemoteAddress, "CLIENT_IP", s.Client.IP, -1)
	assert.Equal(s.T(), expectedLocalAddress, serverNetwork.LocalAddress)
	assert.Equal(s.T(), expectedRemoteAddress, serverNetwork.RemoteAddress)
	assert.Equal(s.T(), "ROLE_SERVER", serverNetwork.Role)
	assert.Equal(s.T(), s.Server.ExpectedNetwork[0].SocketFamily, serverNetwork.SocketFamily)

	serverEndpoints, err := s.GetEndpoints(s.Server.ContainerID)
	s.Require().NoError(err)
	assert.Equal(s.T(), len(s.Server.ExpectedEndpoints), len(serverEndpoints))

	sort.Slice(s.Server.ExpectedEndpoints, func(i, j int) bool {
		return endpointComparison(s.Server.ExpectedEndpoints[i], s.Server.ExpectedEndpoints[j])
	})
	sort.Slice(serverEndpoints, func(i, j int) bool { return endpointComparison(serverEndpoints[i], serverEndpoints[j]) })

	for idx := range serverEndpoints {
		assert.Equal(s.T(), s.Server.ExpectedEndpoints[idx].Protocol, serverEndpoints[idx].Protocol)
		assert.Equal(s.T(), s.Server.ExpectedEndpoints[idx].Address, serverEndpoints[idx].Address)
	}
}

func endpointComparison(endpoint1 common.EndpointInfo, endpoint2 common.EndpointInfo) bool {
	if endpoint1.Address == nil {
		return false
	}
	if endpoint2.Address == nil {
		return true
	}

	if endpoint1.Address.AddressData < endpoint2.Address.AddressData {
		return true
	}

	if endpoint1.Address.AddressData > endpoint2.Address.AddressData {
		return false
	}

	if endpoint1.Address.Port < endpoint2.Address.Port {
		return true
	}

	if endpoint1.Address.Port > endpoint2.Address.Port {
		return false
	}

	if endpoint1.Address.IpNetwork < endpoint2.Address.IpNetwork {
		return true
	}

	if endpoint1.Address.IpNetwork > endpoint2.Address.IpNetwork {
		return false
	}

	if endpoint1.Protocol < endpoint2.Protocol {
		return true
	}

	return false
}
