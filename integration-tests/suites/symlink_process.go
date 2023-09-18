package suites

import (
	"time"

	"github.com/stretchr/testify/assert"

	"github.com/stackrox/collector/integration-tests/suites/common"
	// "github.com/stackrox/collector/integration-tests/suites/types"
)

type SymbolicLinkProcessTestSuite struct {
	IntegrationTestSuiteBase
	serverContainer string
}

func (s *SymbolicLinkProcessTestSuite) SetupSuite() {

	s.metrics = map[string]float64{}
	s.StartContainerStats()

	s.Collector().Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":false,"scrapeInterval":2}`
	s.Collector().Env["ROX_PROCESSES_LISTENING_ON_PORT"] = "true"

	s.StartCollector(false)

	processImage := getProcessListeningOnPortsImage()

	actionFile := "/tmp/action_file_ln.txt"
	_, err := s.executor.Exec("sh", "-c", "rm "+actionFile+" || true")

	containerID, err := s.launchContainer("process-ports", "-v", "/tmp:/tmp", "--entrypoint", "./plop", processImage, actionFile)
	s.Require().NoError(err)

	s.serverContainer = common.ContainerShortID(containerID)

	_, err = s.executor.Exec("sh", "-c", "echo open 9092 > "+actionFile)
	s.Require().NoError(err)
	err = s.waitForFileToBeDeleted(actionFile)
	s.Require().NoError(err)

	time.Sleep(6 * time.Second)
}

func (s *SymbolicLinkProcessTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainer([]string{"process-ports", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("SymbolicLinkProcess", stats, s.metrics)
}

func (s *SymbolicLinkProcessTestSuite) TestSymbolicLinkProcess() {
	// processes := s.Sensor().ExpectNProcesses(s.T(), s.serverContainer, 10*time.Second, 1)
	endpoints := s.Sensor().ExpectEndpointsN(s.T(), s.serverContainer, 10*time.Second, 1)

	//processesMap := make(map[string][]types.ProcessInfo)
	// for _, process := range processes {
	// 	name := process.Name
	// 	processesMap[name] = append(processesMap[name], process)
	// }

	// lnProcess := processesMap["plop"][0]
	assert.Equal(s.T(), "L4_PROTOCOL_TCP", endpoints[0].Protocol)

	assert.Equal(s.T(), endpoints[0].Originator.ProcessName, "plop")
	assert.Equal(s.T(), endpoints[0].Originator.ProcessExecFilePath, "/plop")
	assert.Equal(s.T(), endpoints[0].Originator.ProcessArgs, "/tmp/action_file_ln.txt")
	assert.Equal(s.T(), 9092, endpoints[0].Address.Port)
}
