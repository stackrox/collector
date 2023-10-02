package suites

import (
	"sort"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/config"
	"github.com/stackrox/collector/integration-tests/suites/types"
	"github.com/stretchr/testify/assert"
)

type ProcessListeningOnPortTestSuite struct {
	IntegrationTestSuiteBase
	serverContainer string
}

func (s *ProcessListeningOnPortTestSuite) SetupSuite() {
	s.StartContainerStats()

	collector := s.Collector()

	collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":false,"scrapeInterval":1}`
	collector.Env["ROX_PROCESSES_LISTENING_ON_PORT"] = "true"

	s.StartCollector(false)

	processImage := getProcessListeningOnPortsImage()

	containerID, err := s.launchContainer("process-ports", "-v", "/tmp:/tmp", processImage)
	s.Require().NoError(err)

	s.serverContainer = common.ContainerShortID(containerID)

	actionFile := "/tmp/action_file.txt"

	_, err = s.executor.Exec("sh", "-c", "rm "+actionFile+" || true")

	_, err = s.executor.Exec("sh", "-c", "echo open 8081 > "+actionFile)
	err = s.waitForFileToBeDeleted(actionFile)
	s.Require().NoError(err)

	_, err = s.executor.Exec("sh", "-c", "echo open 9091 > "+actionFile)
	err = s.waitForFileToBeDeleted(actionFile)
	s.Require().NoError(err)

	time.Sleep(6 * time.Second)

	_, err = s.executor.Exec("sh", "-c", "echo close 8081 > "+actionFile)
	err = s.waitForFileToBeDeleted(actionFile)
	s.Require().NoError(err)
	_, err = s.executor.Exec("sh", "-c", "echo close 9091 > "+actionFile)
	err = s.waitForFileToBeDeleted(actionFile)
	s.Require().NoError(err)
}

func (s *ProcessListeningOnPortTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainer([]string{"process-ports", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("ProcessListeningOnPort", stats, s.metrics)
}

func (s *ProcessListeningOnPortTestSuite) TestProcessListeningOnPort() {
	processes := s.Sensor().ExpectProcessesN(s.T(), s.serverContainer, 30*time.Second, 2)
	endpoints := s.Sensor().ExpectEndpointsN(s.T(), s.serverContainer, 30*time.Second, 4)

	// sort by name to ensure processes[0] is the plop process (the other
	// is the shell)
	// All of these asserts check against the processes information of that program.
	sort.Slice(processes, func(i, j int) bool {
		return processes[i].Name < processes[j].Name
	})

	process := processes[0]

	possiblePorts := []int{8081, 9091}

	// First verify that all endpoints have the expected metadata, that
	// they are the correct protocol and come from the expected Originator
	// process.
	for _, endpoint := range endpoints {
		assert.Equal(s.T(), "L4_PROTOCOL_TCP", endpoint.Protocol)

		assert.Equal(s.T(), endpoint.Originator.ProcessName, process.Name)
		assert.Equal(s.T(), endpoint.Originator.ProcessExecFilePath, process.ExePath)
		assert.Equal(s.T(), endpoint.Originator.ProcessArgs, process.Args)

		// assert that we haven't got any unexpected ports - further checking
		// of this data will occur subsequently
		assert.Contains(s.T(), possiblePorts, endpoint.Address.Port)
	}

	// We can't guarantee the order in which collector reports the endpoints.
	// Check that we have precisely two pairs of endpoints, for opening
	// and closing the port. A closed port will have a populated CloseTimestamp

	endpoints8081 := make([]types.EndpointInfo, 0)
	endpoints9091 := make([]types.EndpointInfo, 0)
	for _, endpoint := range endpoints {
		if endpoint.Address.Port == 8081 {
			endpoints8081 = append(endpoints8081, endpoint)
		} else {
			endpoints9091 = append(endpoints9091, endpoint)
		}
		// other ports cannot exist at this point due to previous assertions
	}

	// This helper simplifies the assertions a fair bit, by checking that
	// the recorded endpoints have an open event (CloseTimestamp == nil) and
	// a close event (CloseTimestamp != nil) and not two close events or two open
	// events.
	//
	// It is also agnostic to the order in which the events are reported.
	hasOpenAndClose := func(infos []types.EndpointInfo) bool {
		if !assert.Len(s.T(), infos, 2) {
			return false
		}
		return infos[0].CloseTimestamp != infos[1].CloseTimestamp &&
			(infos[0].CloseTimestamp == types.NilTimestamp || infos[1].CloseTimestamp == types.NilTimestamp)
	}

	assert.True(s.T(), hasOpenAndClose(endpoints8081), "Did not capture open and close events for port 8081")
	assert.True(s.T(), hasOpenAndClose(endpoints9091), "Did not capture open and close events for port 9091")
}

func getProcessListeningOnPortsImage() string {
	return config.Images().QaImageByKey("qa-plop")
}
