package suites

import (
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/config"
	"github.com/stackrox/collector/integration-tests/suites/types"
	"github.com/stretchr/testify/assert"
)

type ProcessesAndEndpointsTestSuite struct {
	IntegrationTestSuiteBase
	container         string
	Executable        string
	Args              []string
	ExpectedEndpoints []types.EndpointInfo
	ExpectedProcesses []types.ProcessInfo
	ContainerName     string
}

func (s *ProcessesAndEndpointsTestSuite) SetupSuite() {
	s.RegisterCleanup(s.ContainerName)
	s.StartContainerStats()

	collectorOptions := common.CollectorStartupOptions{
		Env: map[string]string{
			"ROX_PROCESSES_LISTENING_ON_PORT": "true",
		},
		Config: map[string]any{
			"turnOffScrape": false,
		},
	}

	s.StartCollector(false, &collectorOptions)

	image := config.Images().QaImageByKey("qa-plop")
	cmd := []string{"--entrypoint", s.Executable, image}
	cmd = append(cmd, s.Args...)
	containerID, err := s.launchContainer(s.ContainerName, cmd...)

	s.Require().NoError(err)
	s.container = common.ContainerShortID(containerID)
}

func (s *ProcessesAndEndpointsTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainers(s.ContainerName)
	s.WritePerfResults()
}

func (s *ProcessesAndEndpointsTestSuite) TestProcessesAndEndpoints() {
	processes := s.Sensor().ExpectProcessesN(s.T(), s.container, 20*time.Second, len(s.ExpectedProcesses))
	endpoints := s.Sensor().ExpectEndpointsN(s.T(), s.container, 20*time.Second, len(s.ExpectedEndpoints))

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

	assert.ElementsMatch(s.T(), s.ExpectedProcesses, processes)
}
