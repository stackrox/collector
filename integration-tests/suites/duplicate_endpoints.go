package suites

import (
	"strconv"
	"strings"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/config"
)

type DuplicateEndpointsTestSuite struct {
	IntegrationTestSuiteBase
	serverContainer string
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
	s.StartContainerStats()

	collector := s.Collector()

	collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":false,"scrapeInterval":20}`
	collector.Env["ROX_PROCESSES_LISTENING_ON_PORT"] = "true"

	s.StartCollector(false)

	time.Sleep(30 * time.Second)

	processImage := config.Images().QaImageByKey("qa-socat")

	containerID, err := s.launchContainer("socat", processImage, "TCP-LISTEN:80,fork", "STDOUT")
	s.Require().NoError(err)

	s.serverContainer = common.ContainerShortID(containerID)

	command := []string{"/bin/sh", "-c", "socat TCP-LISTEN:81,fork STDOUT &"}

	_, err = s.execContainer("socat", command)
	s.Require().NoError(err)
	time.Sleep(22 * time.Second)
	s.killSocatProcess(81)

	_, err = s.execContainer("socat", command)

	time.Sleep(20 * time.Second)
}

func (s *DuplicateEndpointsTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainer([]string{"socat", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("DuplicateEndpoints", stats, s.metrics)
}

func (s *DuplicateEndpointsTestSuite) TestDuplicateEndpoints() {
	s.Sensor().ExpectNEndpoints(s.T(), s.serverContainer, 10*time.Second, 2)
}
