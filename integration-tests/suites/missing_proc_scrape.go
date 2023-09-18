package suites

import (
	"os"
	"time"

	"github.com/stretchr/testify/assert"
)

type MissingProcScrapeTestSuite struct {
	IntegrationTestSuiteBase
}

const fakeProcDir = "/tmp/fake-proc"

func (s *MissingProcScrapeTestSuite) SetupSuite() {
	_, err := os.Stat(fakeProcDir)
	if os.IsNotExist(err) {
		err = os.MkdirAll(fakeProcDir, os.ModePerm)
		s.Require().NoError(err, "Failed to create fake proc directory")
	}

	// Mount the fake proc directory created by 'create-fake-proc.sh'
	s.Collector().Mounts["/host/proc:ro"] = fakeProcDir

	// if /etc/hostname is empty collector will read /proc/sys/kernel/hostname
	// which will also be empty because of the fake proc, so collector will exit.
	// to avoid this, set NODE_HOSTNAME
	s.Collector().Env["NODE_HOSTNAME"] = "collector-missing-proc-host"

	s.StartCollector(false)
}

func (s *MissingProcScrapeTestSuite) TestCollectorRunning() {
	collectorRunning, err := s.executor.IsContainerRunning("collector")
	s.Require().NoError(err)
	assert.True(s.T(), collectorRunning, "Collector isn't running")
}

func (s *MissingProcScrapeTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainer([]string{"collector"})
}
