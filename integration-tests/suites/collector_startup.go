package suites

import (
	"github.com/stackrox/collector/integration-tests/suites/config"
)

type CollectorStartupTestSuite struct {
	IntegrationTestSuiteBase

	nginxContainer string
}

func (s *CollectorStartupTestSuite) SetupSuite() {
	s.StartCollector(false)
}

func (s *CollectorStartupTestSuite) TearDownSuite() {
	s.StopCollector()
}

func (s *CollectorStartupTestSuite) SetupTest() {
	container, err := s.launchContainer("nginx", config.Images().ImageByKey("nginx"))
	s.Require().NoError(err)

	s.nginxContainer = container
}

func (s *CollectorStartupTestSuite) TearDownTest() {
	s.cleanupContainer([]string{s.nginxContainer})
}

func (s *CollectorStartupTestSuite) TestCollectorRunning() {
	running, err := s.executor.IsContainerRunning("collector")
	s.Require().NoError(err)
	s.Require().True(running)

	running, err = s.executor.IsContainerRunning("nginx")
	s.Require().NoError(err)
	s.Require().True(running)
}
