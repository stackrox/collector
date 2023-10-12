package suites

import (
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/config"
	"github.com/stackrox/collector/integration-tests/suites/types"
	"github.com/stretchr/testify/assert"
)

type ChangeProcessNameTestSuite struct {
	IntegrationTestSuiteBase
	container         string
	Executable        string
	Args              []string
	ExpectedEndpoints []types.EndpointInfo
	ExpectedProcesses []types.ProcessInfo
	ContainerName     string
}

func (s *ChangeProcessNameTestSuite) SetupSuite() {

	collector := s.Collector()

	collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":false,"scrapeInterval":2}`
	collector.Env["ROX_PROCESSES_LISTENING_ON_PORT"] = "true"

	s.StartCollector(false)

	image := config.Images().QaImageByKey("qa-plop")
	cmd := []string{s.ContainerName, "--entrypoint", s.Executable, image}
	cmd = append(cmd, s.Args...)
	containerID, err := s.launchContainer(cmd...)

	s.Require().NoError(err)
	s.container = common.ContainerShortID(containerID)

	time.Sleep(20 * time.Second)
}

func (s *ChangeProcessNameTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainer([]string{"collector"})
	s.cleanupContainer([]string{s.ContainerName})
}

func (s *ChangeProcessNameTestSuite) TestChangeProcessName() {
	processes := s.Sensor().Processes(s.container)
	endpoints := s.Sensor().Endpoints(s.container)

	assert.Equal(s.T(), len(s.ExpectedEndpoints), len(endpoints))
	assert.Equal(s.T(), len(s.ExpectedProcesses), len(processes))

	minEndpoints := common.Min(len(s.ExpectedEndpoints), len(endpoints))

	types.SortEndpoints(endpoints)

	for idx := 0; idx < minEndpoints; idx++ {
		assert.Equal(s.T(), s.ExpectedEndpoints[idx].Protocol, endpoints[idx].Protocol)
		assert.Equal(s.T(), s.ExpectedEndpoints[idx].Address.AddressData, endpoints[idx].Address.AddressData)
		assert.Equal(s.T(), s.ExpectedEndpoints[idx].Address.Port, endpoints[idx].Address.Port)
		assert.Equal(s.T(), s.ExpectedEndpoints[idx].Address.IpNetwork, endpoints[idx].Address.IpNetwork)
		assert.Equal(s.T(), s.ExpectedEndpoints[idx].IsActive(), endpoints[idx].IsActive())
		assert.Equal(s.T(), s.ExpectedEndpoints[idx].Originator.ProcessName, endpoints[idx].Originator.ProcessName)
		assert.Equal(s.T(), s.ExpectedEndpoints[idx].Originator.ProcessExecFilePath, endpoints[idx].Originator.ProcessExecFilePath)
		assert.Equal(s.T(), s.ExpectedEndpoints[idx].Originator.ProcessArgs, endpoints[idx].Originator.ProcessArgs)
	}

	minProcesses := common.Min(len(s.ExpectedProcesses), len(processes))

	for idx := 0; idx < minProcesses; idx++ {
		assert.Equal(s.T(), s.ExpectedProcesses[idx].Name, processes[idx].Name)
		assert.Equal(s.T(), s.ExpectedProcesses[idx].ExePath, processes[idx].ExePath)
		assert.Equal(s.T(), s.ExpectedProcesses[idx].Args, processes[idx].Args)
	}
}
