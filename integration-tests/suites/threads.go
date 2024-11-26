package suites

import (
	"fmt"
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/types"
	"github.com/stretchr/testify/assert"
)

type ThreadsTestSuite struct {
	IntegrationTestSuiteBase
}

func (s *ThreadsTestSuite) SetupSuite() {
	s.RegisterCleanup("thread-exec")
	s.StartCollector(false, nil)
}

func (s *ThreadsTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainers("thread-exec")
}

// Verify that Collector correctly traces threads, even if created via clone3.
// This should lead to a correct file path, when doing exec from a thread --
// instead of an exec target we should see the parent file path.
func (s *ThreadsTestSuite) TestThreadExec() {
	image := config.Images().QaImageByKey("qa-thread-exec")
	containerID, err := s.Executor().StartContainer(
		config.ContainerStartConfig{
			Name:  "thread-exec",
			Image: image,
		})
	s.Require().NoError(err)

	if finished, _ := s.waitForContainerToExit("thread-exec", containerID, 10*time.Second, 0); finished {
		expectedProcesses := []types.ProcessInfo{
			types.ProcessInfo{
				Name:    "thread_exec",
				ExePath: "/usr/bin/thread_exec",
				Uid:     0,
				Gid:     0,
				Args:    "",
			},
		}

		s.Sensor().ExpectProcesses(s.T(), common.ContainerShortID(containerID),
			10*time.Second, expectedProcesses...)

		logs, err := s.containerLogs("perf-event-open")
		if err != nil {
			fmt.Println(logs)
		}

	} else {
		assert.FailNow(s.T(), "Timeout waiting for thread-exec")
	}

	s.cleanupContainers(containerID)
}
