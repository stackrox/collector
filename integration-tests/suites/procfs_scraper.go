package suites

import (
	"strconv"
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/types"
)

type ProcfsScraperTestSuite struct {
	IntegrationTestSuiteBase
	ServerContainer             string
	TurnOffScrape               bool
	RoxProcessesListeningOnPort bool
	Expected                    []types.EndpointInfo
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

	collectorOptions := collector.StartupOptions{
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
	image := config.Images().QaImageByKey("qa-nginx")

	err := s.executor.PullImage(image)
	s.Require().NoError(err)

	// invokes default nginx
	containerID, err := s.Executor().StartContainer(config.ContainerStartConfig{
		Name:  "nginx",
		Image: image,
	})

	s.Require().NoError(err)
	s.ServerContainer = common.ContainerShortID(containerID)
}

func (s *ProcfsScraperTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainers("nginx")
	s.WritePerfResults()
}

func (s *ProcfsScraperTestSuite) TestProcfsScraper() {
	if len(s.Expected) == 0 {
		// if we expect no endpoints, this Expect function is more precise
		s.Sensor().ExpectEndpointsN(s.T(), s.ServerContainer, 10*time.Second, 0)
	} else {
		s.Sensor().ExpectEndpoints(s.T(), s.ServerContainer, 10*time.Second, s.Expected...)
	}
}
