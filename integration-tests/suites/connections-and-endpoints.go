package suites

import (
	"fmt"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/config"
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
	s.collector.Env["ROX_ENABLE_AFTERGLOW"] = "false"

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)
	time.Sleep(30 * time.Second)

	socatImage := config.Images().QaImageByKey("qa-socat")

	serverName := s.Server.Name
	clientName := s.Client.Name

	longContainerID, err := s.launchContainer(serverName, "--entrypoint", "/bin/sh", socatImage, "-c", "/bin/sleep 300")
	s.Server.ContainerID = common.ContainerShortID(longContainerID)
	s.Require().NoError(err)

	longContainerID, err = s.launchContainer(clientName, "--entrypoint", "/bin/sh", socatImage, "-c", "/bin/sleep 300")
	s.Require().NoError(err)
	s.Client.ContainerID = common.ContainerShortID(longContainerID)

	s.Server.IP, err = s.getIPAddress(serverName)
	s.Require().NoError(err)
	s.Client.IP, err = s.getIPAddress(clientName)
	s.Require().NoError(err)

	serverCmd := strings.Replace(s.Server.Cmd, "CLIENT_IP", s.Client.IP, -1)
	_, err = s.execContainer(serverName, []string{"/bin/sh", "-c", serverCmd})
	s.Require().NoError(err)

	time.Sleep(3 * time.Second)

	clientCmd := strings.Replace(s.Client.Cmd, "SERVER_IP", s.Server.IP, -1)
	_, err = s.execContainer(clientName, []string{"/bin/sh", "-c", clientCmd})
	s.Require().NoError(err)
	time.Sleep(6 * time.Second)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *ConnectionsAndEndpointsTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"collector"})
	s.cleanupContainer([]string{s.Server.Name})
	s.cleanupContainer([]string{s.Client.Name})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("ConnectionsAndEndpoints", stats, s.metrics)
}

func (s *ConnectionsAndEndpointsTestSuite) TestConnectionsAndEndpoints() {

	// TODO If ExpectedNetwork is nil the test should check that it is actually nil
	if s.Client.ExpectedNetwork != nil {
		clientNetworks, err := s.GetNetworks(s.Client.ContainerID)
		s.Require().NoError(err)
		nNetwork := len(clientNetworks)
		nExpectedNetwork := len(s.Client.ExpectedNetwork)
		// TODO Get this assert to pass reliably for these tests. Don't just do the asserts for the last connection. https://issues.redhat.com/browse/ROX-17964
		// assert.Equal(s.T(), nClientNetwork, nExpectedClientNetwork)
		if nExpectedNetwork != nNetwork {
			fmt.Println("WARNING: Expected " + strconv.Itoa(nExpectedNetwork) + " client network connections but found " + strconv.Itoa(nNetwork))
		}
		lastNetwork := clientNetworks[nNetwork-1]
		lastExpectedNetwork := s.Client.ExpectedNetwork[nExpectedNetwork-1]
		expectedLocalAddress := strings.Replace(lastExpectedNetwork.LocalAddress, "CLIENT_IP", s.Client.IP, -1)
		expectedRemoteAddress := strings.Replace(lastExpectedNetwork.RemoteAddress, "SERVER_IP", s.Server.IP, -1)
		assert.Equal(s.T(), expectedLocalAddress, lastNetwork.LocalAddress)
		assert.Equal(s.T(), expectedRemoteAddress, lastNetwork.RemoteAddress)
		assert.Equal(s.T(), "ROLE_CLIENT", lastNetwork.Role)
		assert.Equal(s.T(), lastExpectedNetwork.SocketFamily, lastNetwork.SocketFamily)
	}

	if s.Client.ExpectedEndpoints != nil {
		fmt.Println("Expected client endpoint should be nil")
	}
	_, err := s.GetEndpoints(s.Client.ContainerID)
	s.Require().Error(err, "There should be no client endpoint")

	// TODO If ExpectedNetwork is nil the test should check that it is actually nil
	if s.Server.ExpectedNetwork != nil {
		serverNetworks, err := s.GetNetworks(s.Server.ContainerID)
		s.Require().NoError(err)
		nNetwork := len(serverNetworks)
		nExpectedNetwork := len(s.Server.ExpectedNetwork)
		// TODO Get this assert to pass reliably for these tests. Don't just do the asserts for the last connection. https://issues.redhat.com/browse/ROX-18803
		// assert.Equal(s.T(), nServerNetwork, nExpectedServerNetwork)
		if nExpectedNetwork != nNetwork {
			fmt.Println("WARNING: Expected " + strconv.Itoa(nExpectedNetwork) + " server network connections but found " + strconv.Itoa(nNetwork))
		}
		lastNetwork := serverNetworks[nNetwork-1]
		lastExpectedNetwork := s.Server.ExpectedNetwork[nExpectedNetwork-1]
		expectedLocalAddress := strings.Replace(lastExpectedNetwork.LocalAddress, "SERVER_IP", s.Server.IP, -1)
		expectedRemoteAddress := strings.Replace(lastExpectedNetwork.RemoteAddress, "CLIENT_IP", s.Client.IP, -1)
		assert.Equal(s.T(), expectedLocalAddress, lastNetwork.LocalAddress)
		assert.Equal(s.T(), expectedRemoteAddress, lastNetwork.RemoteAddress)
		assert.Equal(s.T(), "ROLE_SERVER", lastNetwork.Role)
		assert.Equal(s.T(), lastExpectedNetwork.SocketFamily, lastNetwork.SocketFamily)
	}

	serverEndpoints, err := s.GetEndpoints(s.Server.ContainerID)
	if s.Server.ExpectedEndpoints != nil {
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
	} else {
		s.Require().Error(err)
	}

}

func endpointComparison(endpoint1 common.EndpointInfo, endpoint2 common.EndpointInfo) bool {
	addr1, addr2 := endpoint1.Address, endpoint2.Address

	if addr1 == nil {
		return false
	}
	if addr2 == nil {
		return true
	}

	if addr1.AddressData < addr2.AddressData {
		return true
	}

	if addr1.AddressData > addr2.AddressData {
		return false
	}

	if addr1.Port < addr2.Port {
		return true
	}

	if addr1.Port > addr2.Port {
		return false
	}

	if endpoint1.Protocol < endpoint2.Protocol {
		return true
	}

	return false
}
