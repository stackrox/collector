package suites

import (
	"fmt"
	"strconv"
	"strings"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stretchr/testify/assert"
)

type Container struct {
	name             string
	cmd              string
	containerID      string
	listenIP         string
	expectedNetwork  string
	expectedEndpoint []common.EndpointInfo
}

type ServerClientPair struct {
	server Container
	client Container
}

type MixedUpEphemeralPortsTestSuite struct {
	IntegrationTestSuiteBase
	ServerClientPairs []*ServerClientPair
}

func (s *MixedUpEphemeralPortsTestSuite) SetupSuite() {

	s.metrics = map[string]float64{}
	s.executor = common.NewExecutor()
	s.StartContainerStats()
	s.collector = common.NewCollectorManager(s.executor, s.T().Name())

	s.collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":false,"scrapeInterval":2}`
	s.collector.Env["ROX_PROCESSES_LISTENING_ON_PORT"] = "true"
	s.collector.Env["ROX_ENABLE_AFTERGLOW"] = "0"

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)
	time.Sleep(30 * time.Second)

	socatImage := common.QaImage("quay.io/rhacs-eng/qa", "socat")

	// Client uses a port not in the ephemeral ports range as an ephemeral port
	mixedHighLowPorts := &ServerClientPair{
		server: Container{
			cmd:             "socat TCP4-LISTEN:40000,reuseaddr,fork - &",
			expectedNetwork: "ROLE_SERVER",
			expectedEndpoint: []common.EndpointInfo{
				{
					Protocol: "L4_PROTOCOL_TCP",
					Address: &common.ListenAddress{
						AddressData: "0.0.0.0",
						Port:        40000,
					},
				},
			},
		},
		client: Container{
			cmd:              "echo hello | socat - TCP4:LISTEN_IP:40000,sourceport=10000",
			expectedNetwork:  "ROLE_CLIENT",
			expectedEndpoint: nil,
		},
	}

	// Server listens on a port in the ephemeral ports range
	normalPorts := &ServerClientPair{
		server: Container{
			cmd:             "socat TCP4-LISTEN:40,reuseaddr,fork - &",
			expectedNetwork: "ROLE_SERVER",
			expectedEndpoint: []common.EndpointInfo{
				{
					Protocol: "L4_PROTOCOL_TCP",
					Address: &common.ListenAddress{
						AddressData: "0.0.0.0",
						Port:        40,
					},
				},
			},
		},
		client: Container{
			cmd:              "echo hello | socat - TCP4:LISTEN_IP:40",
			expectedNetwork:  "ROLE_CLIENT",
			expectedEndpoint: nil,
		},
	}

	// Client uses a port not in the ephemeral ports range as an ephemeral port and the connection is kept open
	persistentConnection := &ServerClientPair{
		server: Container{
			cmd:             "socat TCP4-LISTEN:50000,reuseaddr,fork - &",
			expectedNetwork: "ROLE_SERVER",
			expectedEndpoint: []common.EndpointInfo{
				{
					Protocol: "L4_PROTOCOL_TCP",
					Address: &common.ListenAddress{
						AddressData: "0.0.0.0",
						Port:        50000,
					},
				},
			},
		},
		client: Container{
			cmd:              "tail -f /dev/null | socat - TCP4:LISTEN_IP:50000,sourceport=20000 &",
			expectedNetwork:  "ROLE_CLIENT",
			expectedEndpoint: nil,
		},
	}

	s.ServerClientPairs = []*ServerClientPair{mixedHighLowPorts, normalPorts, persistentConnection}

	for idx, serverClientPair := range s.ServerClientPairs {
		serverName := "socat-server-" + strconv.Itoa(idx)
		clientName := "socat-client-" + strconv.Itoa(idx)
		serverClientPair.server.name = serverName
		serverClientPair.client.name = clientName
		longContainerID, err := s.launchContainer(serverName, socatImage, "/bin/sh", "-c", "/bin/sleep 300")
		serverClientPair.server.containerID = common.ContainerShortID(longContainerID)
		s.Require().NoError(err)
		longContainerID, err = s.launchContainer(clientName, socatImage, "/bin/sh", "-c", "/bin/sleep 300")
		serverClientPair.client.containerID = common.ContainerShortID(longContainerID)
		_, err = s.execContainer(serverName, []string{"/bin/sh", "-c", serverClientPair.server.cmd})
		s.Require().NoError(err)
		time.Sleep(3 * time.Second)
		serverClientPair.server.listenIP, err = s.getIPAddress(serverName)
		s.Require().NoError(err)
		clientCmd := strings.Replace(serverClientPair.client.cmd, "LISTEN_IP", serverClientPair.server.listenIP, -1)
		_, err = s.execContainer(clientName, []string{"/bin/sh", "-c", clientCmd})
		s.Require().NoError(err)
	}

	time.Sleep(6 * time.Second)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *MixedUpEphemeralPortsTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"collector"})
	for _, serverClientPair := range s.ServerClientPairs {
		s.cleanupContainer([]string{serverClientPair.server.name})
		s.cleanupContainer([]string{serverClientPair.client.name})
	}
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("EphemeralPorts", stats, s.metrics)
}

func (s *MixedUpEphemeralPortsTestSuite) TestMixedUpEphemeralPorts() {
	for _, serverClientPair := range s.ServerClientPairs {
		fmt.Println()
		fmt.Println("serverName= ", serverClientPair.server.name)
		fmt.Println("clientName= ", serverClientPair.client.name)
		fmt.Println("serverCmd= ", serverClientPair.server.cmd)
		fmt.Println("clientCmd= ", serverClientPair.client.cmd)

		serverEndpoints, err := s.GetEndpoints(serverClientPair.server.containerID)
		assert.Equal(s.T(), len(serverClientPair.server.expectedEndpoint), len(serverEndpoints))

		clientEndpoints, err := s.GetEndpoints(serverClientPair.client.containerID)
		s.Require().Error(err)
		assert.Equal(s.T(), len(serverClientPair.client.expectedEndpoint), len(clientEndpoints))

		val, err := s.Get(serverClientPair.client.containerID, networkBucket)
		s.Require().NoError(err)
		actualValues := strings.Split(string(val), "|")
		assert.Equal(s.T(), "ROLE_CLIENT", actualValues[2])

		val, err = s.Get(serverClientPair.server.containerID, networkBucket)
		s.Require().NoError(err)
		actualValues = strings.Split(string(val), "|")
		assert.Equal(s.T(), "ROLE_SERVER", actualValues[2])
	}
}
