package suites

import (
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/config"
)

// GRPCStreamDelayTestSuite exercises Collector in degraded network
// conditions, using a delay.
type GRPCStreamDelayTestSuite struct {
	IntegrationTestSuiteBase
}

func (s *GRPCStreamDelayTestSuite) SetupSuite() {
	s.RegisterCleanup("nginx")

	// Introduce a large delay in the network flows processing
	s.Sensor().SetNetworkStreamDelay(30 * time.Second)

	collectorOptions := collector.StartupOptions{
		Config: map[string]any{
			// Use a short scrape interval so the collector attempts to
			// write network data quickly after the stream is opened.
			"scrapeInterval": 2,
		},
		Env: map[string]string{
			"ROX_COLLECTOR_SCRAPE_DISABLED": "false",
		},
	}

	s.StartCollector(false, &collectorOptions)

	// Start an nginx container to generate network endpoints that the
	// collector will try to report.
	image := config.Images().QaImageByKey("qa-nginx")
	err := s.Executor().PullImage(image)
	s.Require().NoError(err)

	_, err = s.Executor().StartContainer(config.ContainerStartConfig{
		Name:  "nginx",
		Image: image,
		Ports: []uint16{80},
	})
	s.Require().NoError(err)

	// Wait long enough for the collector to:
	// 1. Detect the nginx endpoints via scraping
	// 2. Attempt to write to the not-yet-accepted stream
	// 3. Handle the WaitUntilStarted timeout
	// 4. Eventually reconnect after the delay expires
	time.Sleep(50 * time.Second)
}

func (s *GRPCStreamDelayTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainers("nginx")
}

// TestCollectorSurvivesStreamDelay verifies that the collector does not
// crash when the GRPC server is slow to accept the network connection
// info stream.
func (s *GRPCStreamDelayTestSuite) TestCollectorSurvivesStreamDelay() {
	running, err := s.Collector().IsRunning()
	s.Require().NoError(err)
	s.Assert().True(running, "Collector should still be running after GRPC stream delay")
}
