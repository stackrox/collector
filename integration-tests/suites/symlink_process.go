package suites

import (
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stretchr/testify/assert"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/types"
)

type SymbolicLinkProcessTestSuite struct {
	IntegrationTestSuiteBase
	serverContainer string
}

func (s *SymbolicLinkProcessTestSuite) SetupSuite() {
	s.RegisterCleanup("process-ports")
	s.StartContainerStats()

	collectorOptions := collector.StartupOptions{
		Config: map[string]any{
			"turnOffScrape": false,
		},
		Env: map[string]string{
			"ROX_PROCESSES_LISTENING_ON_PORT": "true",
		},
	}

	s.StartCollector(false, &collectorOptions)

	processImage := getProcessListeningOnPortsImage()

	containerID, err := s.Executor().StartContainer(
		config.ContainerStartConfig{
			Name:  "process-ports",
			Image: processImage,
			Entrypoint: []string{
				"./plop", "--app", "plop.py", "run", "-h", "0.0.0.0",
			},
		})
	s.Require().NoError(err)

	s.serverContainer = common.ContainerShortID(containerID)
}

func (s *SymbolicLinkProcessTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainers("process-ports")
	s.WritePerfResults()
}

func (s *SymbolicLinkProcessTestSuite) TestSymbolicLinkProcess() {
	processes := s.Sensor().ExpectProcessesN(s.T(), s.serverContainer, 10*time.Second, 1)
	endpoints := s.Sensor().ExpectEndpointsN(s.T(), s.serverContainer, 10*time.Second, 1)

	processesMap := make(map[string][]types.ProcessInfo)
	for _, process := range processes {
		name := process.Name
		processesMap[name] = append(processesMap[name], process)
	}

	lnProcess := processesMap["plop"][0]
	assert.Equal(s.T(), "L4_PROTOCOL_TCP", endpoints[0].Protocol)

	assert.Equal(s.T(), endpoints[0].Originator.ProcessName, lnProcess.Name)
	assert.Equal(s.T(), endpoints[0].Originator.ProcessExecFilePath, lnProcess.ExePath)
	assert.Equal(s.T(), endpoints[0].Originator.ProcessArgs, lnProcess.Args)
	// 5000 is the port the flask app listens on for connections.
	assert.Equal(s.T(), 5000, endpoints[0].Address.Port)
}
