package suites

import (
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stretchr/testify/assert"
)

type ProcessListeningOnPortTestSuite struct {
	IntegrationTestSuiteBase
	serverContainer string
}

func (s *ProcessListeningOnPortTestSuite) SetupSuite() {

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

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *ProcessListeningOnPortTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"process-ports", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("ProcessListeningOnPort", stats, s.metrics)
}

func (s *ProcessListeningOnPortTestSuite) TestProcessListeningOnPort() {
	processes, err := s.GetProcesses(s.serverContainer)
	s.Require().NoError(err)
	endpoints, err := s.GetEndpoints(s.serverContainer)
	s.Require().NoError(err)

	if !assert.Equal(s.T(), 4, len(endpoints)) {
		// We can't continue if this is not the case, so panic immediately.
		// It indicates an internal issue with this test and the non-deterministic
		// way in which endpoints are reported.
		assert.FailNowf(s.T(), "", "only retrieved %d endpoints (expect 4)", len(endpoints))
	}

	// Note that the first process is the shell and the second is the process-listening-on-ports program.
	// All of these asserts check against the processes information of that program.
	assert.Equal(s.T(), 2, len(processes))
	process := processes[1]

	possiblePorts := []int{8081, 9091}

	// First verify that all endpoints have the expected metadata, that
	// they are the correct protocol and come from the expected Originator
	// process.
	for _, endpoint := range endpoints {
		assert.Equal(s.T(), "L4_PROTOCOL_TCP", endpoint.Protocol)

		// TODO
		// There is a difference in the lengths of the process name
		// The process stream truncates to 16 characters while endpoints does not.
		// We need to decide which way to go and make these tests consistent with that.
		//assert.Equal(s.T(), endpoint.Originator.ProcessName, process.Name)
		assert.Equal(s.T(), endpoint.Originator.ProcessExecFilePath, process.ExePath)
		assert.Equal(s.T(), endpoint.Originator.ProcessArgs, process.Args)

		// assert that we haven't got any unexpected ports - further checking
		// of this data will occur subsequently
		assert.Contains(s.T(), possiblePorts, endpoint.Address.Port)
	}

	// We can't guarantee the order in which collector reports the endpoints.
	// Check that we have precisely two pairs of endpoints, for opening
	// and closing the port. A closed port will have a populated CloseTimestamp

	endpoints8081 := make([]common.EndpointInfo, 0)
	endpoints9091 := make([]common.EndpointInfo, 0)
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
	hasOpenAndClose := func(infos []common.EndpointInfo) bool {
		if !assert.Len(s.T(), infos, 2) {
			return false
		}
		return infos[0].CloseTimestamp != infos[1].CloseTimestamp &&
			(infos[0].CloseTimestamp == nilTimestamp || infos[1].CloseTimestamp == nilTimestamp)
	}

	assert.True(s.T(), hasOpenAndClose(endpoints8081), "Did not capture open and close events for port 8081")
	assert.True(s.T(), hasOpenAndClose(endpoints9091), "Did not capture open and close events for port 9091")
}

func getProcessListeningOnPortsImage() string {
	return common.QaImage("quay.io/rhacs-eng/qa", "collector-processes-listening-on-ports-3.12.x-11-g64eeab9cbc")
}
