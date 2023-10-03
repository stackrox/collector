package suites

import (
	"fmt"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/config"
	"github.com/stackrox/collector/integration-tests/suites/types"
	"github.com/stretchr/testify/assert"
)

type Container struct {
	Name              string
	Cmd               string
	ContainerID       string
	IP                string
	ExpectedNetwork   []types.NetworkInfo
	ExpectedEndpoints []types.EndpointInfo
}

type ConnectionsAndEndpointsTestSuite struct {
	IntegrationTestSuiteBase
	Server Container
	Client Container
}

func (s *ConnectionsAndEndpointsTestSuite) SetupSuite() {
	defer s.RecoverSetup(s.Server.Name, s.Client.Name)
	s.StartContainerStats()
	collector := s.Collector()

	collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":false,"scrapeInterval":2}`
	collector.Env["ROX_PROCESSES_LISTENING_ON_PORT"] = "true"
	collector.Env["ROX_ENABLE_AFTERGLOW"] = "false"

	s.StartCollector(false)

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
}

func (s *ConnectionsAndEndpointsTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainer([]string{"collector"})
	s.cleanupContainer([]string{s.Server.Name})
	s.cleanupContainer([]string{s.Client.Name})
	s.WritePerfResults("ConnectionsAndEndpoints")
}

func (s *ConnectionsAndEndpointsTestSuite) TestConnectionsAndEndpoints() {

	// TODO If ExpectedNetwork is nil the test should check that it is actually nil
	if s.Client.ExpectedNetwork != nil {
		clientNetworks := s.Sensor().Connections(s.Client.ContainerID)
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

	endpoints := s.Sensor().Endpoints(s.Client.ContainerID)
	assert.Equal(s.T(), 0, len(endpoints))

	// TODO If ExpectedNetwork is nil the test should check that it is actually nil
	if s.Server.ExpectedNetwork != nil {
		serverNetworks := s.Sensor().Connections(s.Server.ContainerID)
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

	serverEndpoints := s.Sensor().Endpoints(s.Server.ContainerID)
	if s.Server.ExpectedEndpoints != nil {
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
		assert.Equal(s.T(), 0, len(serverEndpoints))
	}

}

func endpointComparison(endpoint1 types.EndpointInfo, endpoint2 types.EndpointInfo) bool {
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
