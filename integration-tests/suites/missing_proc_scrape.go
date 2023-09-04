package suites

import (
	"os"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
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

	s.executor = common.NewExecutor()
	s.collector = common.NewCollectorManager(s.executor, s.T().Name())

	s.collector.Mounts["/host/proc:ro"] = fakeProcDir

	// if /etc/hostname is empty collector will read /proc/sys/kernel/hostname
	// which will also be empty because of the fake proc, so collector will exit.
	// to avoid this, set NODE_HOSTNAME
	s.collector.Env["NODE_HOSTNAME"] = "collector-missing-proc-host"

	err = s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)

	time.Sleep(10 * time.Second)
}

func (s *MissingProcScrapeTestSuite) TestCollectorRunning() {
	collectorRunning, err := s.executor.IsContainerRunning("collector")
	s.Require().NoError(err)
	assert.True(s.T(), collectorRunning, "Collector isn't running")
}

func (s *MissingProcScrapeTestSuite) TearDownSuite() {
	err := s.collector.TearDown()
	s.Require().NoError(err)
	s.cleanupContainer([]string{"collector"})
}
