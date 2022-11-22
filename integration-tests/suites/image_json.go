package suites

import (
	"github.com/stackrox/collector/integration-tests/suites/common"
)

type ImageLabelJSONTestSuite struct {
	IntegrationTestSuiteBase
}

func (s *ImageLabelJSONTestSuite) SetupSuite() {
	s.executor = common.NewExecutor()
	s.collector = common.NewCollectorManager(s.executor, s.T().Name())
	err := s.collector.Setup()
	s.Require().NoError(err)
	err = s.collector.Launch()
	s.Require().NoError(err)
}

func (s *ImageLabelJSONTestSuite) TestRunImageWithJSONLabel() {
	s.RunImageWithJSONLabels()
}

func (s *ImageLabelJSONTestSuite) TearDownSuite() {
	err := s.collector.TearDown()
	s.Require().NoError(err)
	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
	s.cleanupContainer([]string{"collector", "grpc-server", "jsonlabel"})
}
