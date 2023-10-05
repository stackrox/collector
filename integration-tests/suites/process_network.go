package suites

import (
	"fmt"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/config"
	"github.com/stackrox/collector/integration-tests/suites/types"
)

type ProcessNetworkTestSuite struct {
	IntegrationTestSuiteBase
	clientContainer string
	clientIP        string
	serverContainer string
	serverIP        string
	serverPort      string
}

// Launches collector
// Launches gRPC server in insecure mode
// Launches nginx container
// Execs into nginx and does a sleep
func (s *ProcessNetworkTestSuite) SetupSuite() {
	defer s.RecoverSetup("nginx", "nginx-curl")
	s.StartContainerStats()
	s.StartCollector(false, nil)

	image_store := config.Images()

	images := []string{
		image_store.ImageByKey("nginx"),
		image_store.ImageByKey("curl"),
	}

	for _, image := range images {
		err := s.executor.PullImage(image)
		s.Require().NoError(err)
	}

	// invokes default nginx
	containerID, err := s.launchContainer("nginx", image_store.ImageByKey("nginx"))
	s.Require().NoError(err)
	s.serverContainer = common.ContainerShortID(containerID)

	// invokes "sleep" and "sh" and "ls"
	_, err = s.execContainer("nginx", []string{"sleep", "5"})
	s.Require().NoError(err)
	_, err = s.execContainer("nginx", []string{"sh", "-c", "ls"})
	s.Require().NoError(err)

	// invokes another container
	containerID, err = s.launchContainer("nginx-curl", image_store.ImageByKey("curl"), "sleep", "300")
	s.Require().NoError(err)
	s.clientContainer = common.ContainerShortID(containerID)

	s.serverIP, err = s.getIPAddress("nginx")
	s.Require().NoError(err)

	s.serverPort, err = s.getPort("nginx")
	s.Require().NoError(err)

	_, err = s.execContainer("nginx-curl", []string{"curl", fmt.Sprintf("%s:%s", s.serverIP, s.serverPort)})
	s.Require().NoError(err)

	s.clientIP, err = s.getIPAddress("nginx-curl")
	s.Require().NoError(err)
}

func (s *ProcessNetworkTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainer([]string{"nginx", "nginx-curl"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("process_network", stats, s.metrics)
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
		types.NetworkInfo{
			LocalAddress:  fmt.Sprintf("%s:%d", s.clientIP, 0),
			RemoteAddress: fmt.Sprintf(":%s", s.serverPort),
		},
	)

	s.Sensor().ExpectConnections(s.T(), s.clientContainer, 10*time.Second,
		types.NetworkInfo{
			LocalAddress:  "",
			RemoteAddress: fmt.Sprintf("%s:%s", s.serverIP, s.serverPort),
		},
	)
}
