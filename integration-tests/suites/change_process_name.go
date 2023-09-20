package suites

import (
	"fmt"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/config"
	"github.com/stretchr/testify/assert"
)

type ChangeProcessNameTestSuite struct {
	IntegrationTestSuiteBase
	serverContainer string
}

func (s *ChangeProcessNameTestSuite) SetupSuite() {

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

	changeProcessNameImage := config.Images().QaImageByKey("qa-change-process-name")

	containerID, err := s.launchContainer("changeProcessName", changeProcessNameImage)

	s.Require().NoError(err)
	s.serverContainer = common.ContainerShortID(containerID)

	time.Sleep(20 * time.Second)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *ChangeProcessNameTestSuite) TearDownSuite() {
	//s.cleanupContainer([]string{"changeProcessName", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("ChangeProcessName", stats, s.metrics)
}

func (s *ChangeProcessNameTestSuite) TestChangeProcessName() {
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
