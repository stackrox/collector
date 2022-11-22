package suites

import (
	"os"
	"strconv"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
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
	s.executor = common.NewExecutor()
	s.StartContainerStats()
	s.collector = common.NewCollectorManager(s.executor, s.T().Name())

	s.collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":` + strconv.FormatBool(s.TurnOffScrape) + `,"scrapeInterval":2}`
	s.collector.Env["ROX_PROCESSES_LISTENING_ON_PORT"] = strconv.FormatBool(s.RoxProcessesListeningOnPort)

	s.launchNginx()

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)
	time.Sleep(10 * time.Second)

	s.cleanupContainer([]string{"nginx"})
	time.Sleep(10 * time.Second)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *ProcfsScraperTestSuite) launchNginx() {
	image := "nginx:1.14-alpine"

	err := s.executor.PullImage(image)
	s.Require().NoError(err)

	// invokes default nginx
	containerID, err := s.launchContainer("nginx", image)
	s.Require().NoError(err)
	s.ServerContainer = common.ContainerShortID(containerID)

	time.Sleep(10 * time.Second)
}

func (s *ProcfsScraperTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"nginx", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("ProcfsScraper", stats, s.metrics)
}

func (s *ProcfsScraperTestSuite) TestProcfsScraper() {
	var imageFamily = os.Getenv("IMAGE_FAMILY")
	// TODO: These tests fail for rhel-7, ubuntu-pro-1804-lts, sles-12, and sometimes for ubuntu-2204-lts.
	// Make the tests pass for them and remove this if statement
	if imageFamily != "rhel-7" && imageFamily != "ubuntu-pro-1804-lts" && imageFamily != "sles-12" && imageFamily != "ubuntu-2204-lts" {
		endpoints, err := s.GetEndpoints(s.ServerContainer)
		if !s.TurnOffScrape && s.RoxProcessesListeningOnPort {
			// If scraping is on and the feature flag is enables we expect to find the nginx endpoint
			s.Require().NoError(err)
			assert.Equal(s.T(), 2, len(endpoints))
			processes, err := s.GetProcesses(s.ServerContainer)
			s.Require().NoError(err)

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
			// we expect not to find the nginx endpoint and we should get an error
			s.Require().Error(err)
		}
	}
}
