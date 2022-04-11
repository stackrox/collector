package integrationtests

import (
	"testing"
	"github.com/stretchr/testify/suite"
)

type ImageLabelJSONTestSuite struct {
	IntegrationTestSuiteBase
}

func TestImageLabelJSON(t *testing.T) {
	suite.Run(t, new(ImageLabelJSONTestSuite))
}

func (s *ImageLabelJSONTestSuite) SetupSuite() {
	s.executor = NewExecutor()
	s.docker = NewDocker(s.executor)
	s.collector = NewCollectorManager(s.executor, s.T().Name())
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
	s.cleanupContainers("collector", "grpc-server", "jsonlabel")
}
