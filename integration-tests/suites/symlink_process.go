package suites

import (
	"time"

	"github.com/stretchr/testify/assert"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/types"
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

	time.Sleep(30 * time.Second)

	processImage := getProcessListeningOnPortsImage()

	actionFile := "/tmp/action_file_ln.txt"
	_, err := s.executor.Exec("sh", "-c", "rm "+actionFile+" || true")

	containerID, err := s.launchContainer("process-ports", "-v", "/tmp:/tmp", "--entrypoint", "./plop", processImage, actionFile)

	s.Require().NoError(err)
	s.serverContainer = common.ContainerShortID(containerID)

	_, err = s.executor.Exec("sh", "-c", "echo open 9092 > "+actionFile)
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
	processes := s.Sensor().Processes(s.serverContainer)
	endpoints := s.Sensor().Endpoints(s.serverContainer)

	if !assert.Equal(s.T(), 1, len(endpoints)) {
		// We can't continue if this is not the case, so panic immediately.
		// It indicates an internal issue with this test and the non-deterministic
		// way in which endpoints are reported.
		assert.FailNowf(s.T(), "", "retrieved %d endpoints (expect 1)", len(endpoints))
	}

	assert.Equal(s.T(), 1, len(processes))

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
	assert.Equal(s.T(), 9092, endpoints[0].Address.Port)
}
