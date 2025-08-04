package suites

import (
	"fmt"
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/types"

	sensorAPI "github.com/stackrox/rox/generated/internalapi/sensor"
	"github.com/stackrox/rox/generated/storage"
)

type ProcessNetworkTestSuite struct {
	IntegrationTestSuiteBase
	clientContainer string
	clientIP        string
	serverContainer string
	serverIP        string
	serverPort      uint32
}

// Launches collector
// Launches gRPC server in insecure mode
// Launches nginx container
// Execs into nginx and does a sleep
func (s *ProcessNetworkTestSuite) SetupSuite() {
	s.RegisterCleanup("nginx", "nginx-curl")
	s.StartCollector(false, nil)

	image_store := config.Images()

	images := []string{
		image_store.QaImageByKey("qa-nginx"),
		image_store.QaImageByKey("qa-alpine-curl"),
	}

	for _, image := range images {
		err := s.Executor().PullImage(image)
		s.Require().NoError(err)
	}

	// invokes default nginx
	containerID, err := s.Executor().StartContainer(
		config.ContainerStartConfig{
			Name:  "nginx",
			Image: image_store.QaImageByKey("qa-nginx"),
			Ports: []uint16{80},
		})

	s.Require().NoError(err)
	s.serverContainer = common.ContainerShortID(containerID)

	// invokes "sleep" and "sh" and "ls"
	_, err = s.execContainer("nginx", []string{"sleep", "5"}, false)
	s.Require().NoError(err)
	_, err = s.execContainer("nginx", []string{"sh", "-c", "ls"}, false)
	s.Require().NoError(err)

	// invokes another container
	containerID, err = s.Executor().StartContainer(
		config.ContainerStartConfig{
			Name:    "nginx-curl",
			Image:   image_store.QaImageByKey("qa-alpine-curl"),
			Command: []string{"sleep", "300"},
		})
	s.Require().NoError(err)
	s.clientContainer = common.ContainerShortID(containerID)

	s.serverIP, err = s.getIPAddress("nginx")
	s.Require().NoError(err)

	ports, err := s.getPorts("nginx")
	s.serverPort = ports[0]
	s.Require().NoError(err)

	_, err = s.execContainer("nginx-curl", []string{"curl", fmt.Sprintf("%s:%d", s.serverIP, s.serverPort)}, false)
	s.Require().NoError(err)

	s.clientIP, err = s.getIPAddress("nginx-curl")
	s.Require().NoError(err)
}

func (s *ProcessNetworkTestSuite) TearDownSuite() {
	s.WritePerfResults()
}

func (s *ProcessNetworkTestSuite) TestProcessViz() {
	expectedProcesses := []types.ProcessInfo{
		types.ProcessInfo{
			Name:    "ls",
			ExePath: "/bin/ls",
			Uid:     0,
			Gid:     0,
			Args:    "",
		},
		types.ProcessInfo{
			Name:    "nginx",
			ExePath: "/usr/sbin/nginx",
			Uid:     0,
			Gid:     0,
			Args:    "-g daemon off;",
		},
		types.ProcessInfo{
			Name:    "sh",
			ExePath: "/bin/sh",
			Uid:     0,
			Gid:     0,
			Args:    "-c ls",
		},
		types.ProcessInfo{
			Name:    "sleep",
			ExePath: "/bin/sleep",
			Uid:     0,
			Gid:     0,
			Args:    "5",
		},
	}

	s.Sensor().ExpectProcesses(s.T(), s.serverContainer, 10*time.Second, expectedProcesses...)
}

func (s *ProcessNetworkTestSuite) TestProcessLineageInfo() {
	expectedLineages := []types.ProcessLineage{
		types.ProcessLineage{
			Name:          "awk",
			ExePath:       "/usr/bin/awk",
			ParentUid:     0,
			ParentExePath: "/usr/bin/bash",
		},
		types.ProcessLineage{
			Name:          "grep",
			ExePath:       "/usr/bin/grep",
			ParentUid:     0,
			ParentExePath: "/usr/bin/bash",
		},
		types.ProcessLineage{
			Name:          "sleep",
			ExePath:       "/usr/bin/sleep",
			ParentUid:     0,
			ParentExePath: "/usr/bin/bash",
		},
	}

	for _, expected := range expectedLineages {
		s.Sensor().ExpectLineages(s.T(), s.serverContainer, 10*time.Second, expected.Name, expected)
	}
}

func (s *ProcessNetworkTestSuite) TestNetworkFlows() {
	s.Sensor().ExpectConnections(s.T(), s.serverContainer, 10*time.Second,
		&sensorAPI.NetworkConnection{
			LocalAddress:   types.CreateNetworkAddress("", "", s.serverPort),
			RemoteAddress:  types.CreateNetworkAddress(s.clientIP, "", 0),
			Protocol:       storage.L4Protocol_L4_PROTOCOL_TCP,
			Role:           sensorAPI.ClientServerRole_ROLE_SERVER,
			SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
			ContainerId:    s.serverContainer,
			CloseTimestamp: nil,
		},
	)

	s.Sensor().ExpectConnections(s.T(), s.clientContainer, 10*time.Second,
		&sensorAPI.NetworkConnection{
			LocalAddress:   nil,
			RemoteAddress:  types.CreateNetworkAddress(s.serverIP, "", s.serverPort),
			Protocol:       storage.L4Protocol_L4_PROTOCOL_TCP,
			Role:           sensorAPI.ClientServerRole_ROLE_CLIENT,
			SocketFamily:   sensorAPI.SocketFamily_SOCKET_FAMILY_UNKNOWN,
			ContainerId:    s.clientContainer,
			CloseTimestamp: nil,
		},
	)
}
