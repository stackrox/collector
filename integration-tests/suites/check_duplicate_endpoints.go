package suites

import (
	"fmt"
	"strconv"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stretchr/testify/assert"
)

type CheckDuplicateEndpointsTestSuite struct {
	IntegrationTestSuiteBase
	serverContainer string
}

func (s *CheckDuplicateEndpointsTestSuite) waitForEndpoints() {
	_, err := s.GetEndpoints(s.serverContainer)
	count := 0
	maxCount := 60

	for err != nil {
		time.Sleep(1 * time.Second)
		_, err = s.GetEndpoints(s.serverContainer)
		count += 1
		if count == maxCount {
			break
		}
	}
}

func (s *CheckDuplicateEndpointsTestSuite) killSocatProcess(port int) {
	pid, err := s.execContainer("socat", []string{"/bin/sh", "-c", "ps | grep socat.*LISTEN:" + strconv.Itoa(port) + ",fork | head -1 | awk '{print $1}'"})
	s.Require().NoError(err)
	fmt.Println("pid= " + pid)
	_, err = s.execContainer("socat", []string{"/bin/sh", "-c", "kill -9 " + pid})
	s.Require().NoError(err)
}

// The desired time line of events in this test is the following. Start a process that opens a port.
// When this endpoint is reported in the mock grpc server we know that a scrape has occured. This is
// time 0. We then start another process that opens a different port, 81 slightly after t=0.
// The scrape interval is 8 seconds so at t=8 the endpoint with port 81 should be reported. At t=10
// seconds the process is killed. At t=10 seconds an identical command that opened port 81 is
// executed. At t=16 at the second scrape interval nothing should be reported, since the port was
// opened and closed in the same interval. We expect there to be two reported endpoints.
func (s *CheckDuplicateEndpointsTestSuite) SetupSuite() {

	s.metrics = map[string]float64{}
	s.executor = common.NewExecutor()
	s.StartContainerStats()
	s.collector = common.NewCollectorManager(s.executor, s.T().Name())

	s.collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":false,"scrapeInterval":8}`
	s.collector.Env["ROX_PROCESSES_LISTENING_ON_PORT"] = "true"

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)
	time.Sleep(30 * time.Second)

	processImage := common.QaImage("quay.io/rhacs-eng/qa", "socat")

	containerID, err := s.launchContainer("socat", processImage, "/bin/sh", "-c", "socat TCP-LISTEN:80,fork STDOUT")

	s.Require().NoError(err)
	s.serverContainer = common.ContainerShortID(containerID)
	s.waitForEndpoints()

	_, err = s.execContainer("socat", []string{"/bin/sh", "-c", "socat TCP-LISTEN:81,fork STDOUT &"})
	s.Require().NoError(err)
	time.Sleep(10 * time.Second)
	s.killSocatProcess(81)
	_, err = s.execContainer("socat", []string{"/bin/sh", "-c", "socat TCP-LISTEN:81,fork STDOUT &"})
	s.Require().NoError(err)

	time.Sleep(20 * time.Second)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *CheckDuplicateEndpointsTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"socat", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("CheckDuplicateEndpoints", stats, s.metrics)
}

func (s *CheckDuplicateEndpointsTestSuite) TestCheckDuplicateEndpoints() {
	processes, err := s.GetProcesses(s.serverContainer)
	s.Require().NoError(err)
	endpoints, err := s.GetEndpoints(s.serverContainer)
	s.Require().NoError(err)

	if !assert.Equal(s.T(), 2, len(endpoints)) {
		// We can't continue if this is not the case, so panic immediately.
		// It indicates an internal issue with this test and the non-deterministic
		// way in which endpoints are reported.
		assert.FailNowf(s.T(), "", "only retrieved %d endpoints (expect 2)", len(endpoints))
	}

	assert.Equal(s.T(), 12, len(processes))
}
