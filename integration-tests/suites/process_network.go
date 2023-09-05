package suites

import (
	"fmt"
	// "sort"
	"time"

	"github.com/stretchr/testify/assert"

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

	s.metrics = map[string]float64{}
	s.executor = common.NewExecutor()
	s.StartContainerStats()
	s.collector = common.NewCollectorManager(s.executor, s.T().Name())

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)

	image_store := config.Images()

	images := []string{
		image_store.ImageByKey("nginx"),
		image_store.ImageByKey("curl"),
	}

	for _, image := range images {
		err := s.executor.PullImage(image)
		s.Require().NoError(err)
	}

	//	time.Sleep(10 * time.Second)

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

	//time.Sleep(10 * time.Second)

	//err = s.collector.TearDown()
	//s.Require().NoError(err)

	//s.db, err = s.collector.BoltDB()
	//s.Require().NoError(err)
}

func (s *ProcessNetworkTestSuite) TearDownSuite() {
	_ = s.collector.TearDown()
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
			Args:    "",
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

	s.collector.ExpectProcesses(s.T(), s.serverContainer, 30*time.Second, expectedProcesses...)
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
		val, err := s.GetLineageInfo(expected.Name, "0", processLineageInfoBucket)
		s.Require().NoError(err)
		lineage, err := types.NewProcessLineage(val)
		s.Require().NoError(err)

		assert.Equal(s.T(), expected, *lineage)
	}
}

func (s *ProcessNetworkTestSuite) TestNetworkFlows() {

	// Server side checks

	s.collector.ExpectNetworks(s.T(), s.serverContainer, 10*time.Second,
		types.NetworkInfo{
			LocalAddress:  fmt.Sprintf("%s:%d", s.clientIP, 0),
			RemoteAddress: fmt.Sprintf(":%s", s.serverPort),
		},
	)

	s.collector.ExpectNetworks(s.T(), s.clientContainer, 10*time.Second,
		types.NetworkInfo{
			LocalAddress:  "",
			RemoteAddress: fmt.Sprintf("%s:%s", s.serverIP, s.serverPort),
		},
	)

	// NetworkSignalHandler does not currently report endpoints.
	// ProcfsScraper, which scrapes networking information from /proc reports endpoints and connections
	// However NetworkSignalHandler, which gets networking information from Falco only reports connections.
	// At some point in the future NetworkSignalHandler will report endpoints and connections.
	// At that time this test and the similar test for the client container will need to be changed.
	// The requirement should be NoError, instead of Error and there should be multiple asserts to
	// check that the endpoints are what we expect them to be.
	//	_, err := s.GetEndpoints(s.serverContainer)
	//	s.Require().Error(err)
	//
	//	networkInfos, err := s.GetNetworks(s.serverContainer)
	//	s.Require().NoError(err)
	//
	//	assert.Equal(s.T(), 1, len(networkInfos))
	//
	//	actualServerEndpoint := networkInfos[0].LocalAddress
	//	actualClientEndpoint := networkInfos[0].RemoteAddress
	//
	//	// From server perspective, network connection info only has local port and remote IP
	//	assert.Equal(s.T(), fmt.Sprintf(":%s", s.serverPort), actualServerEndpoint)
	//	assert.Equal(s.T(), s.clientIP, actualClientEndpoint)
	//
	//	fmt.Printf("ServerDetails from Bolt: %s %+v\n", s.serverContainer, networkInfos[0])
	//	fmt.Printf("ServerDetails from test: %s %s, Port: %s\n", s.serverContainer, s.serverIP, s.serverPort)
	//
	//	// client side checks
	//
	//	// NetworkSignalHandler does not currently report endpoints.
	//	// See the comment above for the server container endpoint test for more info.
	//	_, err = s.GetEndpoints(s.clientContainer)
	//	s.Require().Error(err)
	//
	//	networkInfos, err = s.GetNetworks(s.clientContainer)
	//	s.Require().NoError(err)
	//
	//	assert.Equal(s.T(), 1, len(networkInfos))
	//
	//	actualClientEndpoint = networkInfos[0].LocalAddress
	//	actualServerEndpoint = networkInfos[0].RemoteAddress
	//
	//	// From client perspective, network connection info has no local endpoint and full remote endpoint
	//	assert.Empty(s.T(), actualClientEndpoint)
	//	assert.Equal(s.T(), fmt.Sprintf("%s:%s", s.serverIP, s.serverPort), actualServerEndpoint)
	//
	//	fmt.Printf("ClientDetails from Bolt: %s %+v\n", s.serverContainer, networkInfos[0])
	//	fmt.Printf("ClientDetails from test: %s %s\n", s.clientContainer, s.clientIP)
}
