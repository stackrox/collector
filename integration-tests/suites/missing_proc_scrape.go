package suites

import (
	"os"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stretchr/testify/assert"
)

type MissingProcScrapeTestSuite struct {
	IntegrationTestSuiteBase
}

const fakeProcDir = "/tmp/fake-proc"

func (s *MissingProcScrapeTestSuite) SetupSuite() {
	s.RegisterCleanup()

	_, err := os.Stat(fakeProcDir)
	if os.IsNotExist(err) {
		err = os.MkdirAll(fakeProcDir, os.ModePerm)
		s.Require().NoError(err, "Failed to create fake proc directory")
	}

	collectorOptions := common.CollectorStartupOptions{
		Mounts: map[string]string{
			"/host/proc:ro": fakeProcDir,
		},
		Env: map[string]string{
			// if /etc/hostname is empty collector will read /proc/sys/kernel/hostname
			// which will also be empty because of the fake proc, so collector will exit.
			// to avoid this, set NODE_HOSTNAME
			"NODE_HOSTNAME": "collector-missing-proc-host",
		},
	}

	s.StartCollector(false, &collectorOptions)
}

func (s *MissingProcScrapeTestSuite) TestCollectorRunning() {
	collectorRunning, err := s.executor.IsContainerRunning("collector")
	s.Require().NoError(err)
	assert.True(s.T(), collectorRunning, "Collector isn't running")
}

func (s *MissingProcScrapeTestSuite) TearDownSuite() {
	s.StopCollector()
}
