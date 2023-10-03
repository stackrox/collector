package suites

type CollectorStartupTestSuite struct {
	IntegrationTestSuiteBase
}

func (s *CollectorStartupTestSuite) SetupSuite() {
	defer s.RecoverSetup()
	s.StartCollector(false, nil)
}

func (s *CollectorStartupTestSuite) TearDownSuite() {
	s.StopCollector()
}

func (s *CollectorStartupTestSuite) TestCollectorRunning() {
	running, err := s.executor.IsContainerRunning("collector")
	s.Require().NoError(err)
	s.Require().True(running)
}
