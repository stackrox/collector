package suites

import (
	"fmt"
	"strconv"
	"strings"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/config"
	"github.com/stretchr/testify/assert"
)

type DuplicateEndpointsTestSuite struct {
	IntegrationTestSuiteBase
	serverContainer string
}

func (s *DuplicateEndpointsTestSuite) waitForEndpoints() {
	s.executor.CopyFromHost(s.collector.DBPathRemote, s.collector.DBPath)
	s.db, _ = s.collector.BoltDB()
	_, err := s.GetEndpoints(s.serverContainer)
	count := 0
	maxCount := 60

	for err != nil {
		time.Sleep(1 * time.Second)
		s.executor.CopyFromHost(s.collector.DBPathRemote, s.collector.DBPath)
		_, err = s.GetEndpoints(s.serverContainer)
		count += 1
		if count == maxCount {
			fmt.Println("Timedout waiting for endpoints")
			break
		}
	}
}

func (s *DuplicateEndpointsTestSuite) killSocatProcess(port int) {
	// Example output    13 root      0:00 socat TCP-LISTEN:81,fork STDOUT
	output, err := s.execContainer("socat", []string{"/bin/sh", "-c", "ps | grep socat.*LISTEN:" + strconv.Itoa(port) + ",fork | head -1"})
	// When running remotely there are quotes around the response
	outputReplaced := strings.Replace(output, "\"", "", -1)
	outputTrimmed := strings.Trim(outputReplaced, " ")
	pid := strings.Split(outputTrimmed, " ")[0]
	s.Require().NoError(err)
	_, err = s.execContainer("socat", []string{"/bin/sh", "-c", "kill -9 " + pid})
	s.Require().NoError(err)
}

// The desired time line of events in this test is the following. Start a process that opens a port.
// When this endpoint is reported in the mock grpc server we know that a scrape has occured. This is
// time 0.
//
// 1. Start a process that opens a different port, 81, slight after t=0
// 2. At t=20 (the scape interval), port 81 should be reported
// 3. At t=22, kill the process
// 4. At t=22, start an identical process that opens port 81
// 5. At t=40 (the second scrape) nothing should be reported.
//
// The test expects only two reported endpoints.
func (s *DuplicateEndpointsTestSuite) SetupSuite() {

	s.metrics = map[string]float64{}
	s.executor = common.NewExecutor()
	s.StartContainerStats()
	s.collector = common.NewCollectorManager(s.executor, s.T().Name())

	s.collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":false,"scrapeInterval":20}`
	s.collector.Env["ROX_PROCESSES_LISTENING_ON_PORT"] = "true"

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)
	time.Sleep(30 * time.Second)

	processImage := config.Images().QaImageByKey("qa-socat")

	containerID, err := s.launchContainer("socat", processImage, "TCP-LISTEN:80,fork", "STDOUT")

	s.Require().NoError(err)
	s.serverContainer = common.ContainerShortID(containerID)
	s.waitForEndpoints()

	command := []string{"/bin/sh", "-c", "socat TCP-LISTEN:81,fork STDOUT &"}

	_, err = s.execContainer("socat", command)
	s.Require().NoError(err)
	time.Sleep(22 * time.Second)
	s.killSocatProcess(81)

	_, err = s.execContainer("socat", command)

	time.Sleep(20 * time.Second)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *DuplicateEndpointsTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"socat", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("DuplicateEndpoints", stats, s.metrics)
}

func (s *DuplicateEndpointsTestSuite) TestDuplicateEndpoints() {
	processes, err := s.GetProcesses(s.serverContainer)
	s.Require().NoError(err)
	endpoints, err := s.GetEndpoints(s.serverContainer)
	s.Require().NoError(err)

	assert.Equal(s.T(), 2, len(endpoints))
	assert.Equal(s.T(), 10, len(processes))
}
