package suites

import "github.com/stackrox/collector/integration-tests/pkg/collector"

type RingBufferTestSuite struct {
	IntegrationTestSuiteBase
}

func (s *RingBufferTestSuite) SetupSuite() {
	// Set the totall allowed ring buffer size to 400Mb. On a box with 128 CPU
	// cores this should trigger an adjustment making the singular ring buffer
	// size 2Mb (without the power-of-two alignment it would be an invalid
	// value of 3Mb)
	collectorOptions := collector.StartupOptions{
		Env: map[string]string{
			"ROX_COLLECTOR_SINSP_TOTAL_BUFFER_SIZE": "419430400",
		},
	}

	s.StartCollector(false, &collectorOptions)
}

func (s *RingBufferTestSuite) TearDownSuite() {
	s.StopCollector()
}

func (s *RingBufferTestSuite) TestCollectorRunning() {
	running, err := s.executor.IsContainerRunning("collector")
	s.Require().NoError(err)
	s.Require().True(running)
}
