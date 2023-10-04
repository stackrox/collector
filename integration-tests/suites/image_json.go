package suites

import (
	"github.com/stackrox/collector/integration-tests/suites/config"
)

type ImageLabelJSONTestSuite struct {
	IntegrationTestSuiteBase
}

func (s *ImageLabelJSONTestSuite) SetupSuite() {
	defer s.RecoverSetup()
	s.StartCollector(false)
}

func (s *ImageLabelJSONTestSuite) TestRunImageWithJSONLabel() {
	name := "jsonlabel"
	image := config.Images().QaImageByKey("performance-json-label")

	err := s.Executor().PullImage(image)
	s.Require().NoError(err)

	containerID, err := s.launchContainer(name, image)
	s.Require().NoError(err)

	_, err = s.waitForContainerToExit(name, containerID, defaultWaitTickSeconds)
	s.Require().NoError(err)
}

func (s *ImageLabelJSONTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainers("json-label")
}
