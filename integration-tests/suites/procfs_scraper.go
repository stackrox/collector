package suites

import (
	"strconv"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/config"
	"github.com/stretchr/testify/assert"
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

	s.metrics = map[string]float64{}
	s.StartContainerStats()

	s.Collector().Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":` + strconv.FormatBool(s.TurnOffScrape) + `,"scrapeInterval":2}`
	s.Collector().Env["ROX_PROCESSES_LISTENING_ON_PORT"] = strconv.FormatBool(s.RoxProcessesListeningOnPort)

	s.launchNginx()

	time.Sleep(10 * time.Second)

	s.StartCollector(false)

	s.cleanupContainer([]string{"nginx"})
}

func (s *ProcfsScraperTestSuite) launchNginx() {
	image := config.Images().ImageByKey("nginx")

	err := s.executor.PullImage(image)
	s.Require().NoError(err)

	// invokes default nginx
	containerID, err := s.launchContainer("nginx", image)
	s.Require().NoError(err)
	s.ServerContainer = common.ContainerShortID(containerID)

	time.Sleep(10 * time.Second)
}

func (s *ProcfsScraperTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainer([]string{"nginx", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("ProcfsScraper", stats, s.metrics)
}

func (s *ProcfsScraperTestSuite) TestProcfsScraper() {
	endpoints := s.Sensor().Endpoints(s.ServerContainer)
	if !s.TurnOffScrape && s.RoxProcessesListeningOnPort {
		if len(endpoints) != 2 {
			assert.FailNowf(s.T(), "incorrect number of endpoints reported", "Expected 2 endpoints, got %d", len(endpoints))
		}

		// If scraping is on and the feature flag is enables we expect to find the nginx endpoint
		processes := s.Sensor().Processes(s.ServerContainer)

		assert.Equal(s.T(), "L4_PROTOCOL_TCP", endpoints[0].Protocol)
		assert.Equal(s.T(), "(timestamp: nil Timestamp)", endpoints[0].CloseTimestamp)
		assert.Equal(s.T(), endpoints[0].Originator.ProcessName, processes[0].Name)
		assert.Equal(s.T(), endpoints[0].Originator.ProcessExecFilePath, processes[0].ExePath)
		assert.Equal(s.T(), endpoints[0].Originator.ProcessArgs, processes[0].Args)

		assert.Equal(s.T(), "L4_PROTOCOL_TCP", endpoints[1].Protocol)
		assert.NotEqual(s.T(), "(timestamp: nil Timestamp)", endpoints[1].CloseTimestamp)
		assert.Equal(s.T(), endpoints[1].Originator.ProcessName, processes[0].Name)
		assert.Equal(s.T(), endpoints[1].Originator.ProcessExecFilePath, processes[0].ExePath)
		assert.Equal(s.T(), endpoints[1].Originator.ProcessArgs, processes[0].Args)
	} else {
		// If scraping is off or the feature flag is disabled
		// we expect not to find the nginx endpoint
		assert.Equal(s.T(), 0, len(endpoints))
	}
}
