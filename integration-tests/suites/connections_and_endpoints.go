package suites

import (
	"strconv"
	"strings"
	"time"

	sensorAPI "github.com/stackrox/rox/generated/internalapi/sensor"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/log"
	"github.com/stackrox/collector/integration-tests/pkg/types"
	"github.com/stretchr/testify/assert"
)

type Container struct {
	Name              string
	Cmd               string
	ContainerID       string
	IP                string
	ExpectedNetwork   []*sensorAPI.NetworkConnection
	ExpectedEndpoints []types.EndpointInfo
}

type ConnectionsAndEndpointsTestSuite struct {
	IntegrationTestSuiteBase
	Server Container
	Client Container
}

func (s *ConnectionsAndEndpointsTestSuite) SetupSuite() {
	s.RegisterCleanup(s.Server.Name, s.Client.Name)
	s.StartContainerStats()

	collectorOptions := collector.StartupOptions{
		Env: map[string]string{
			"ROX_PROCESSES_LISTENING_ON_PORT": "true",
			"ROX_ENABLE_AFTERGLOW":            "false",
		},
		Config: map[string]any{
			"turnOffScrape": false,
		},
	}

	s.StartCollector(false, &collectorOptions)

	socatImage := config.Images().QaImageByKey("qa-socat")
	err := s.executor.PullImage(socatImage)
	s.Require().NoError(err)

	serverName := s.Server.Name
	clientName := s.Client.Name

	longContainerID, err := s.Executor().StartContainer(
		config.ContainerStartConfig{Name: serverName,
			Entrypoint: []string{"/bin/sh"},
			Image:      socatImage,
			Command:    []string{"-c", "/bin/sleep 300"},
		})
	s.Require().NoError(err)
	s.Server.ContainerID = common.ContainerShortID(longContainerID)

	longContainerID, err = s.Executor().StartContainer(
		config.ContainerStartConfig{Name: clientName,
			Entrypoint: []string{"/bin/sh"},
			Image:      socatImage,
			Command:    []string{"-c", "/bin/sleep 300"},
		})
	s.Require().NoError(err)
	s.Client.ContainerID = common.ContainerShortID(longContainerID)

	s.Server.IP, err = s.getIPAddress(serverName)
	s.Require().NoError(err)
	s.Client.IP, err = s.getIPAddress(clientName)
	s.Require().NoError(err)

	serverCmd := strings.Replace(s.Server.Cmd, "CLIENT_IP", s.Client.IP, -1)
	_, err = s.execContainer(serverName, []string{"/bin/sh", "-c", serverCmd}, true)
	s.Require().NoError(err)

	common.Sleep(3 * time.Second)

	clientCmd := strings.Replace(s.Client.Cmd, "SERVER_IP", s.Server.IP, -1)
	_, err = s.execContainer(clientName, []string{"/bin/sh", "-c", clientCmd}, true)
	s.Require().NoError(err)
	common.Sleep(6 * time.Second)
}

func (s *ConnectionsAndEndpointsTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainers(s.Server.Name, s.Client.Name)
	s.WritePerfResults()
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
			log.Warn("Expected " + strconv.Itoa(nExpectedNetwork) + " client network connections but found " + strconv.Itoa(nNetwork))
		}
		lastNetwork := clientNetworks[nNetwork-1]
		lastExpectedNetwork := s.Client.ExpectedNetwork[nExpectedNetwork-1]
		lastExpectedNetwork.RemoteAddress = types.CreateNetworkAddress(s.Server.IP, "", lastExpectedNetwork.RemoteAddress.Port)
		lastExpectedNetwork.ContainerId = s.Client.ContainerID

		assert.True(s.T(), types.EqualNetworkConnection(*lastExpectedNetwork, *lastNetwork))
	}

	if s.Client.ExpectedEndpoints != nil {
		log.Warn("Expected client endpoint should be nil")
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
			log.Warn("Expected " + strconv.Itoa(nExpectedNetwork) + " server network connections but found " + strconv.Itoa(nNetwork))
		}
		lastNetwork := serverNetworks[nNetwork-1]
		lastExpectedNetwork := s.Server.ExpectedNetwork[nExpectedNetwork-1]
		lastExpectedNetwork.RemoteAddress = types.CreateNetworkAddress(s.Client.IP, "", lastExpectedNetwork.RemoteAddress.Port)
		lastExpectedNetwork.ContainerId = s.Server.ContainerID

		assert.True(s.T(), types.EqualNetworkConnection(*lastExpectedNetwork, *lastNetwork))
	}

	serverEndpoints := s.Sensor().Endpoints(s.Server.ContainerID)
	if s.Server.ExpectedEndpoints != nil {
		assert.Equal(s.T(), len(s.Server.ExpectedEndpoints), len(serverEndpoints))

		types.SortEndpoints(s.Server.ExpectedEndpoints)
		types.SortEndpoints(serverEndpoints)

		for idx := range serverEndpoints {
			assert.Equal(s.T(), s.Server.ExpectedEndpoints[idx].Protocol, serverEndpoints[idx].Protocol)
			assert.Equal(s.T(), s.Server.ExpectedEndpoints[idx].Address, serverEndpoints[idx].Address)
		}
	} else {
		assert.Equal(s.T(), 0, len(serverEndpoints))
	}

}
