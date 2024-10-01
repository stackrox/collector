package suites

import (
	"strconv"
	"strings"
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/log"
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
	err := s.executor.PullImage(image)
	s.Require().NoError(err)
	// attach to sched:sched_process_exit and count events
	containerID, err := s.Executor().StartContainer(
		config.ContainerStartConfig{
			Name:       "perf-event-open",
			Image:      image,
			Privileged: true,
			Command:    []string{"", "STDOUT"},
		})
	s.Require().NoError(err)

	if finished, _ := s.waitForContainerToExit("perf-event-open", containerID, 5*time.Second, 0); finished {
		logs, err := s.containerLogs("perf-event-open")
		if err != nil {
			log.Info(logs.GetSingleLog())
			assert.FailNow(s.T(), "Failed to initialize host for performance testing")
		}

		count, err := strconv.Atoi(strings.TrimSpace(logs.GetSingleLog()))
		if err != nil {
			log.Info(logs.GetSingleLog())
			assert.FailNow(s.T(), "Cannot convert result to the integer type")
		}

		s.Assert().Greater(count, 0, "Number of captured tracepoint events is zero")
	}
	s.cleanupContainers(containerID)
}
