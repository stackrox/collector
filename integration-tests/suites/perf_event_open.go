package suites

import (
	"fmt"
	"strconv"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/config"
	"github.com/stretchr/testify/assert"
)

type PerfEventOpenTestSuite struct {
	IntegrationTestSuiteBase
}

func (s *PerfEventOpenTestSuite) SetupSuite() {
	s.RegisterCleanup("perf-event-open")
	s.StartCollector(false, nil)
}

func (s *PerfEventOpenTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainers("perf-event-open")
}

// Verify that Collector probe doesn't block tracepoints
func (s *PerfEventOpenTestSuite) TestReadingTracepoints() {
	image := config.Images().QaImageByKey("qa-perf-event-open")
	// attach to sched:sched_process_exit and count events
	containerID, err := s.launchContainer("perf-event-open", image, "", "STDOUT")
	s.Require().NoError(err)

	if finished, _ := s.waitForContainerToExit("perf-event-open", containerID, 5*time.Second); finished {
		logs, err := s.containerLogs("perf-event-open")
		if err != nil {
			fmt.Println(logs)
			assert.FailNow(s.T(), "Failed to initialize host for performance testing")
		}

		count, err := strconv.Atoi(logs)
		if err != nil {
			fmt.Println(logs)
			assert.FailNow(s.T(), "Cannot convert result to the integer type")
		}

		s.Assert(s.T(), count > 0, "Number of captured tracepoint events is zero")
	}
	s.cleanupContainers(containerID)
}
