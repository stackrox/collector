package integrationtests

import (
	"os"
	"time"
	"testing"
	"github.com/stretchr/testify/suite"
	"github.com/stretchr/testify/assert"
)

type MissingProcScrapeTestSuite struct {
	IntegrationTestSuiteBase
}

// TestMissingProcScrape only works with local fake proc directory
func TestMissingProcScrape(t *testing.T) {
	if ReadEnvVarWithDefault("REMOTE_HOST_TYPE", "local") == "local" {
		suite.Run(t, new(MissingProcScrapeTestSuite))
	}
}

func (s *MissingProcScrapeTestSuite) SetupSuite() {
	_, err := os.Stat("/tmp/fake-proc")
	assert.False(s.T(), os.IsNotExist(err), "Missing fake proc directory")

	s.executor = NewExecutor()
	s.docker = NewDocker(s.executor)
	s.collector = NewCollectorManager(s.executor, s.T().Name())

	// Mount the fake proc directory created by 'create-fake-proc.sh'
	s.collector.Mounts["/host/proc:ro"] = "/tmp/fake-proc"

	err = s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)

	time.Sleep(10 * time.Second)
}

func (s *MissingProcScrapeTestSuite) TestCollectorRunning() {
	collectorRunning, err := s.docker.IsContainerRunning("collector")
	s.Require().NoError(err)
	assert.True(s.T(), collectorRunning, "Collector isn't running")
}

func (s *MissingProcScrapeTestSuite) TearDownSuite() {
	err := s.collector.TearDown()
	s.Require().NoError(err)
	s.cleanupContainers("collector")
}
