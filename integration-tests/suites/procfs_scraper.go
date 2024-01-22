package suites

import (
	"strconv"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/config"
	"github.com/stackrox/collector/integration-tests/suites/types"
)

type ProcfsScraperTestSuite struct {
	IntegrationTestSuiteBase
	ServerContainer             string
	TurnOffScrape               bool
	RoxProcessesListeningOnPort bool
}

// Launches nginx container
// Launches gRPC server in insecure mode
// Launches collector
// Note it is important to launch the nginx container before collector, which is the opposite of
// other tests. The purpose is that we want ProcfsScraper to see the nginx endpoint and we do not want
// NetworkSignalHandler to see the nginx endpoint.
func (s *ProcfsScraperTestSuite) SetupSuite() {
	s.RegisterCleanup("nginx")

	s.StartContainerStats()

	collectorOptions := common.CollectorStartupOptions{
		Env: map[string]string{
			"ROX_PROCESSES_LISTENING_ON_PORT": strconv.FormatBool(s.RoxProcessesListeningOnPort),
		},
		Config: map[string]any{
			"turnOffScrape": s.TurnOffScrape,
		},
	}

	s.launchNginx()

	s.StartCollector(false, &collectorOptions)

	s.cleanupContainers("nginx")
}

func (s *ProcfsScraperTestSuite) launchNginx() {
	image := config.Images().ImageByKey("nginx")

	err := s.executor.PullImage(image)
	s.Require().NoError(err)

	// invokes default nginx
	containerID, err := s.launchContainer("nginx", image)
	s.Require().NoError(err)
	s.ServerContainer = common.ContainerShortID(containerID)
}

func (s *ProcfsScraperTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainers("nginx")
	s.WritePerfResults()
}

func (s *ProcfsScraperTestSuite) TestProcfsScraper() {
	if !s.TurnOffScrape && s.RoxProcessesListeningOnPort {
		// If scraping is on and the feature flag is enables we expect to find the nginx endpoint
		processes := s.Sensor().Processes(s.ServerContainer)

		s.Sensor().ExpectEndpoints(s.T(), s.ServerContainer, 10*time.Second, types.EndpointInfo{
			Protocol:       "L4_PROTOCOL_TCP",
			CloseTimestamp: types.NilTimestamp,
			Originator: &types.ProcessOriginator{
				ProcessName:         processes[0].Name,
				ProcessExecFilePath: processes[0].ExePath,
				ProcessArgs:         processes[0].Args,
			},
		}, types.EndpointInfo{
			Protocol:       "L4_PROTOCOL_TCP",
			CloseTimestamp: types.NilTimestamp,
			Originator: &types.ProcessOriginator{
				ProcessName:         processes[0].Name,
				ProcessExecFilePath: processes[0].ExePath,
				ProcessArgs:         processes[0].Args,
			},
		})
	} else {
		// If scraping is off or the feature flag is disabled
		// we expect to find the endpoint but with no originator process
		s.Sensor().ExpectEndpoints(s.T(), s.ServerContainer, 10*time.Second, types.EndpointInfo{
			Protocol:       "L4_PROTOCOL_TCP",
			CloseTimestamp: types.NilTimestamp,
			Originator: &types.ProcessOriginator{
				ProcessName:         "",
				ProcessExecFilePath: "",
				ProcessArgs:         "",
			},
		})
	}
}
