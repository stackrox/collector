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

	changeProcessNameImage := config.Images().QaImageByKey("qa-plop")
	containerID, err := s.launchContainer("change-process-name", "--entrypoint", "./change-process-name", changeProcessNameImage)

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

	assert.Equal(s.T(), 1, len(endpoints))
	assert.Equal(s.T(), 1, len(processes))

	assert.Equal(s.T(), "L4_PROTOCOL_TCP", endpoints[0].Protocol)
	assert.Equal(s.T(), endpoints[0].Originator.ProcessName, processes[0].Name)
	assert.Equal(s.T(), endpoints[0].Originator.ProcessExecFilePath, processes[0].ExePath)
	assert.Equal(s.T(), endpoints[0].Originator.ProcessArgs, processes[0].Args)
	assert.Equal(s.T(), 8082, endpoints[0].Address.Port)

	assert.Equal(s.T(), processes[0].Name, "change-process-")
	assert.Equal(s.T(), processes[0].Uid, 0)
	assert.Equal(s.T(), processes[0].Gid, 0)
	assert.Equal(s.T(), processes[0].Args, "")
}
