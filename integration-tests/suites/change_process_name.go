package suites

import (
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

	collector := s.Collector()

	collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":false,"scrapeInterval":2}`
	collector.Env["ROX_PROCESSES_LISTENING_ON_PORT"] = "true"

	s.StartCollector(false)

	changeProcessNameImage := config.Images().QaImageByKey("qa-plop")
	// TODO pass arbitrary commands here
	containerID, err := s.launchContainer("change-process-name", "--entrypoint", "./change-process-name", changeProcessNameImage)

	s.Require().NoError(err)
	s.serverContainer = common.ContainerShortID(containerID)

	time.Sleep(20 * time.Second)
}

func (s *ChangeProcessNameTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"change-process-name", "collector"})
}

func (s *ChangeProcessNameTestSuite) TestChangeProcessName() {
	processes := s.Sensor().Processes(s.serverContainer)
	endpoints := s.Sensor().Endpoints(s.serverContainer)

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
