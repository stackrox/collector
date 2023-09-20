package suites

import (
	"fmt"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/config"
	"github.com/stretchr/testify/assert"
)

type SwitchProcessesTestSuite struct {
	IntegrationTestSuiteBase
	serverContainer string
}

func (s *SwitchProcessesTestSuite) SetupSuite() {

	s.metrics = map[string]float64{}
	s.executor = common.NewExecutor()
	s.StartContainerStats()
	s.collector = common.NewCollectorManager(s.executor, s.T().Name())

	s.collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":false,"scrapeInterval":2}`
	s.collector.Env["ROX_PROCESSES_LISTENING_ON_PORT"] = "true"

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)
	time.Sleep(30 * time.Second)

	switchProcessesImage := config.Images().QaImageByKey("qa-switch-processes")

	containerID, err := s.launchContainer("switchProcesses", switchProcessesImage)

	s.Require().NoError(err)
	s.serverContainer = common.ContainerShortID(containerID)

	time.Sleep(20 * time.Second)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *SwitchProcessesTestSuite) TearDownSuite() {
	//s.cleanupContainer([]string{"switchProcesses", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("SwitchProcesses", stats, s.metrics)
}

func (s *SwitchProcessesTestSuite) TestSwitchProcesses() {
	processes, err := s.GetProcesses(s.serverContainer)
	s.Require().NoError(err)
	endpoints, err := s.GetEndpoints(s.serverContainer)
	s.Require().NoError(err)

	for _, process := range processes {
		fmt.Printf("%+v \n", process)
	}
	fmt.Println()
	fmt.Println()
	fmt.Println()
	for _, endpoint := range endpoints {
		fmt.Printf("%+v \n", endpoint)
		fmt.Printf("%+v \n", *endpoint.Originator)
	}

	assert.Equal(s.T(), 1, len(endpoints))
	assert.Equal(s.T(), 3, len(processes))
}
