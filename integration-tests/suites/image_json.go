package suites

import (
	"github.com/stackrox/collector/integration-tests/pkg/config"
)

type ImageLabelJSONTestSuite struct {
	IntegrationTestSuiteBase
}

func (s *ImageLabelJSONTestSuite) SetupSuite() {
	s.RegisterCleanup()
	s.StartCollector(false, nil)
}

func (s *ImageLabelJSONTestSuite) TestRunImageWithJSONLabel() {
	name := "jsonlabel"
	image := config.Images().QaImageByKey("performance-json-label")

	ex := s.Executor()
	err := ex.PullImage(image)
	s.Require().NoError(err)

	containerID, err := s.launchContainer(name, image)
	s.Require().NoError(err)

	_, err = s.waitForContainerToExit(name, containerID, defaultWaitTickSeconds, 0)
	s.Require().NoError(err)
}

func (s *ImageLabelJSONTestSuite) TearDownSuite() {
	s.cleanupContainers("jsonlabel")
}
