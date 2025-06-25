package suites

import (
	"fmt"
	"strconv"
	"time"

	sensorAPI "github.com/stackrox/rox/generated/internalapi/sensor"
	"github.com/stackrox/rox/generated/storage"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/log"
	"github.com/stackrox/collector/integration-tests/pkg/types"
)

const (
	UDP_CLIENT = "udp-client"
	UDP_SERVER = "udp-server"

	// The number of containers to be created in
	// multi destination/source tests
	CONTAINER_COUNT = 3
)

type UdpNetworkFlow struct {
	IntegrationTestSuiteBase
	DNSEnabled bool
}

type containerData struct {
	id   string
	ip   string
	port uint32
}

func (c *containerData) String() string {
	out := fmt.Sprintf("%s: %s", c.id, c.ip)
	if c.port != 0 {
		out += fmt.Sprintf(":%d", c.port)
	}

	return out
}

func (s *UdpNetworkFlow) SetupSuite() {
	// The network needs to be removed after the containers, so its
	// the first cleanup we register.
	s.RegisterCleanup(UDP_CLIENT, UDP_SERVER)
	s.StartContainerStats()
	collectorOptions := collector.StartupOptions{
		Env: map[string]string{
			"ROX_COLLECTOR_TRACK_SEND_RECV": "true",
		},
	}
	s.StartCollector(false, &collectorOptions)

	image := config.Images().QaImageByKey("qa-udp")

	err := s.Executor().PullImage(image)
	s.Require().NoError(err)
}

func (s *UdpNetworkFlow) AfterTest(suiteName, testName string) {
	containers := []string{
		UDP_CLIENT,
		UDP_SERVER,
	}

	for i := 0; i < CONTAINER_COUNT; i++ {
		containers = append(containers, fmt.Sprintf("%s-%d", UDP_SERVER, i))
	}

	for i := 0; i < CONTAINER_COUNT; i++ {
		containers = append(containers, fmt.Sprintf("%s-%d", UDP_CLIENT, i))
	}
	s.cleanupContainers(containers...)
}

func (s *UdpNetworkFlow) TearDownSubTest() {
	s.cleanupContainers(UDP_CLIENT, UDP_SERVER)
}

func (s *UdpNetworkFlow) TearDownSuite() {
	s.WritePerfResults()
}

func (s *UdpNetworkFlow) TestUdpNetorkflow() {
	sendSyscalls := []string{"sendto", "sendmsg", "sendmmsg"}
	recvSyscalls := []string{"recvfrom", "recvmsg", "recvmmsg"}
	image := config.Images().QaImageByKey("qa-udp")

	port := uint32(9090)
	for _, send := range sendSyscalls {
		for _, recv := range recvSyscalls {
			testName := fmt.Sprintf("%s_%s", send, recv)
			s.Run(testName, func() {
				s.runTest(image, recv, send, port)
			})

			port++
		}
	}
}

func (s *UdpNetworkFlow) runTest(image, recv, send string, port uint32) {
	server := s.runServer(config.ContainerStartConfig{
		Name:    UDP_SERVER,
		Image:   image,
		Command: newServerCmd(recv, port),
	}, port)
	client := s.runClient(config.ContainerStartConfig{
		Name:       UDP_CLIENT,
		Image:      image,
		Command:    newClientCmd(send, "", "", server),
		Entrypoint: []string{"udp-client"},
	})
	log.Info("Server: %s - Client: %s\n", server.String(), client.String())

	clientConnection := &sensorAPI.NetworkConnection{
		LocalAddress:   types.CreateNetworkAddress("", "", 0),
		RemoteAddress:  types.CreateNetworkAddress(server.ip, "", server.port),
		Role:           sensorAPI.ClientServerRole_ROLE_CLIENT,
		SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
		CloseTimestamp: nil,
	}

	// Expected server connection
	serverConnection := &sensorAPI.NetworkConnection{
		LocalAddress:   types.CreateNetworkAddress("", "", server.port),
		RemoteAddress:  types.CreateNetworkAddress(client.ip, "", 0),
		Role:           sensorAPI.ClientServerRole_ROLE_SERVER,
		SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
		CloseTimestamp: nil,
	}

	s.Sensor().ExpectConnections(s.T(), client.id, 5*time.Second, clientConnection)
	s.Sensor().ExpectConnections(s.T(), server.id, 5*time.Second, serverConnection)
}

func (s *UdpNetworkFlow) TestMultipleDestinations() {
	image := config.Images().QaImageByKey("qa-udp")

	servers := make([]containerData, CONTAINER_COUNT)
	clientConnections := make([]*sensorAPI.NetworkConnection, CONTAINER_COUNT)
	for i := 0; i < CONTAINER_COUNT; i++ {
		name := fmt.Sprintf("%s-%d", UDP_SERVER, i)
		port := uint32(9000 + i)
		servers[i] = s.runServer(config.ContainerStartConfig{
			Name:    name,
			Image:   image,
			Command: newServerCmd("recvfrom", port),
		}, port)
		log.Info("Server: %s\n", servers[i].String())

		// Load the client connection collector has to send for this server.
		clientConnections[i] = &sensorAPI.NetworkConnection{
			LocalAddress:   types.CreateNetworkAddress("", "", 0),
			RemoteAddress:  types.CreateNetworkAddress(servers[i].ip, "", servers[i].port),
			Protocol:       storage.L4Protocol_L4_PROTOCOL_UDP,
			Role:           sensorAPI.ClientServerRole_ROLE_CLIENT,
			SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
			CloseTimestamp: nil,
		}
	}

	// We give a big period here to ensure the syscall happens just once
	// Due to an implementation restriction, the total number of messages
	// sent must be less than 32.
	client := s.runClient(config.ContainerStartConfig{
		Name:       UDP_CLIENT,
		Image:      image,
		Command:    newClientCmd("sendmmsg", "300", "8", servers...),
		Entrypoint: []string{"udp-client"},
	})
	log.Info("Client: %s\n", client.String())

	for _, server := range servers {
		serverConnection := &sensorAPI.NetworkConnection{
			LocalAddress:   types.CreateNetworkAddress("", "", server.port),
			RemoteAddress:  types.CreateNetworkAddress(client.ip, "", 0),
			Protocol:       storage.L4Protocol_L4_PROTOCOL_UDP,
			Role:           sensorAPI.ClientServerRole_ROLE_SERVER,
			SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
			CloseTimestamp: nil,
		}
		s.Sensor().ExpectConnections(s.T(), server.id, 5*time.Second, serverConnection)
	}
	s.Sensor().ExpectConnections(s.T(), client.id, 5*time.Second, clientConnections...)
}

func (s *UdpNetworkFlow) TestMultipleSources() {
	image := config.Images().QaImageByKey("qa-udp")
	port := uint32(9100)

	server := s.runServer(config.ContainerStartConfig{
		Name:    UDP_SERVER,
		Image:   image,
		Command: newServerCmd("recvmmsg", port),
	}, port)
	log.Info("Server: %s\n", server.String())

	clients := make([]containerData, CONTAINER_COUNT)
	serverConnections := make([]*sensorAPI.NetworkConnection, CONTAINER_COUNT)
	for i := 0; i < CONTAINER_COUNT; i++ {
		name := fmt.Sprintf("%s-%d", UDP_CLIENT, i)
		clients[i] = s.runClient(config.ContainerStartConfig{
			Name:       name,
			Image:      image,
			Command:    newClientCmd("sendto", "300", "", server),
			Entrypoint: []string{"udp-client"},
		})
		log.Info("Client: %s\n", clients[i].String())

		// Load the server connection collector has to send for this client.
		serverConnections[i] = &sensorAPI.NetworkConnection{
			LocalAddress:   types.CreateNetworkAddress("", "", server.port),
			RemoteAddress:  types.CreateNetworkAddress(clients[i].ip, "", 0),
			Protocol:       storage.L4Protocol_L4_PROTOCOL_UDP,
			Role:           sensorAPI.ClientServerRole_ROLE_SERVER,
			SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
			CloseTimestamp: nil,
		}
	}

	clientConnection := &sensorAPI.NetworkConnection{
		LocalAddress:   types.CreateNetworkAddress("", "", 0),
		RemoteAddress:  types.CreateNetworkAddress(server.ip, "", server.port),
		Protocol:       storage.L4Protocol_L4_PROTOCOL_UDP,
		Role:           sensorAPI.ClientServerRole_ROLE_CLIENT,
		SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
		CloseTimestamp: nil,
	}

	for _, client := range clients {
		s.Sensor().ExpectConnections(s.T(), client.id, 5*time.Second, clientConnection)
	}
	s.Sensor().ExpectConnections(s.T(), server.id, 5*time.Second, serverConnections...)
}

func newServerCmd(recv string, port uint32) []string {
	return []string{
		"--syscall", recv,
		"--port", strconv.FormatUint(uint64(port), 10),
	}
}

func (s *UdpNetworkFlow) runServer(cfg config.ContainerStartConfig, port uint32) containerData {
	return s.runContainer(cfg, port)
}

func newClientCmd(send, period, msgs string, servers ...containerData) []string {
	cmd := []string{
		"--syscall", send,
	}

	if period != "" {
		cmd = append(cmd, "--period", period)
	}

	if msgs != "" {
		cmd = append(cmd, "--messages", msgs)
	}

	for _, server := range servers {
		serverLocation := server.ip
		cmd = append(cmd, fmt.Sprintf("%s:%d", serverLocation, server.port))
	}

	return cmd
}

func (s *UdpNetworkFlow) runClient(cfg config.ContainerStartConfig) containerData {
	return s.runContainer(cfg, 0)
}

func (s *UdpNetworkFlow) runContainer(cfg config.ContainerStartConfig, port uint32) containerData {
	id, err := s.Executor().StartContainer(cfg)
	s.Require().NoError(err)

	ip, err := s.getIPAddress(cfg.Name)
	s.Require().NoError(err)

	return containerData{
		id:   common.ContainerShortID(id),
		ip:   ip,
		port: port,
	}
}
