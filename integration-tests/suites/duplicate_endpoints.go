package suites

import (
	"fmt"
	"strconv"
	"strings"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/config"
)

const (
	gScrapeInterval = 10
)

type DuplicateEndpointsTestSuite struct {
	IntegrationTestSuiteBase
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

func (s *DuplicateEndpointsTestSuite) SetupSuite() {
	s.StartContainerStats()

	collector := s.Collector()

	collector.Env["COLLECTOR_CONFIG"] = fmt.Sprintf(`{"logLevel":"debug","turnOffScrape":false,"scrapeInterval":%d}`, gScrapeInterval)
	collector.Env["ROX_PROCESSES_LISTENING_ON_PORT"] = "true"

	s.StartCollector(false)
}

func (s *DuplicateEndpointsTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainer([]string{"socat"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("DuplicateEndpoints", stats, s.metrics)
}

// https://issues.redhat.com/browse/ROX-13628
//
// Endpoints are only reported by collector as a result of procfs scraping,
// therefore this test is strictly concerned with the scrape interval.
//
// The desired time line of events in this test is the following:
//
// for time (t), and scrape interval (i)
//
// 1. Start a process that opens a port, 80
// 2. Wait for the endpoint to be reported, this is t=0
// 3. Start a process that opens a different port, 81
// 4. At t=i (the scape interval), port 81 should be reported
// 5. At t=(i+2), kill the process
// 6. At t=(i+2), start an identical process that opens port 81
// 7. At t=2i (the second scrape) nothing should be reported.
//
// The test expects only two reported endpoints.
func (s *DuplicateEndpointsTestSuite) TestDuplicateEndpoints() {
	image := config.Images().QaImageByKey("qa-socat")
	// (1) start a process that opens port 80
	containerID, err := s.launchContainer("socat", image, "TCP-LISTEN:80,fork", "STDOUT")
	s.Require().NoError(err)

	containerID = common.ContainerShortID(containerID)
	socatCommand := []string{
		"/bin/sh", "-c", "socat TCP-LISTEN:81,fork STDOUT &",
	}

	// (2) wait for the endpoint to be reported
	// wait up to twice the scrape interval, to avoid flakes
	s.Sensor().ExpectEndpointsN(s.T(), containerID, 2*gScrapeInterval*time.Second, 1)

	// (3) start the new endpoint, opening a different port
	_, err = s.execContainer("socat", socatCommand)
	s.Require().NoError(err)

	// (4) wait for the endpoint to be reported
	// expecting two endpoints because that is the total expected for the container
	s.Sensor().ExpectEndpointsN(s.T(), containerID, gScrapeInterval*time.Second, 2)

	// (5) kill the process after a delay
	time.Sleep(2 * time.Second)
	s.killSocatProcess(81)

	// (6) start an idential process
	_, err = s.execContainer("socat", socatCommand)
	s.Require().NoError(err)

	// (7) wait for another scrape interval, and verify we have still only
	// seen 2 endpoints
	time.Sleep(gScrapeInterval * time.Second)
	s.Assert().Len(s.Sensor().Endpoints(containerID), 2, "Got more endpoints than expected")

	// additional final check to ensure there are no additional reports
	s.Assert().Len(s.Sensor().Processes(containerID), 8, "Got more processes than expected")
}
