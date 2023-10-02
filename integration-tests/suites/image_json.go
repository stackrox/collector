package suites

type ImageLabelJSONTestSuite struct {
	IntegrationTestSuiteBase
}

func (s *ImageLabelJSONTestSuite) SetupSuite() {
	defer s.RecoverSetup()
	s.StartCollector(false)
}

func (s *ImageLabelJSONTestSuite) TestRunImageWithJSONLabel() {
	s.RunImageWithJSONLabels()
}

func (s *ImageLabelJSONTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainer([]string{"grpc-server", "jsonlabel"})
}
