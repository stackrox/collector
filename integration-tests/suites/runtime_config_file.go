package suites

import (
	"fmt"
	"strconv"
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stretchr/testify/assert"
)

type RuntimeConfigFileTestSuite struct {
	IntegrationTestSuiteBase
}

// Launches collector
func (s *RuntimeConfigFileTestSuite) SetupSuite() {
	s.RegisterCleanup("nginx", "nginx-curl")
	s.StartContainerStats()

	collectorOptions := collector.StartupOptions{
		Config: map[string]any{
			// turnOffScrape will be true, but the scrapeInterval
			// also controls the reporting interval for network events
			"scrapeInterval": s.ScrapeInterval,
		},
		Env: map[string]string{
			"ROX_AFTERGLOW_PERIOD": "0",
			"ROX_ENABLE_AFTERGLOW": "false",
		},
	}

	s.StartCollector(false, &collectorOptions)

}

func (s *RuntimeConfigFileTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainers("nginx", "nginx-curl")
	s.WritePerfResults()
}

func (s *RuntimeConfigFileTestSuite) TestRuntimeConfigFile() {
	networkInfos := s.Sensor().ExpectConnectionsN(s.T(), s.ServerContainer, 10*time.Second, s.ExpectedActive+s.ExpectedInactive)

	observedActive := 0
	observedInactive := 0

	for _, info := range networkInfos {
		if info.IsActive() {
			observedActive++
		} else {
			observedInactive++
		}
	}

	assert.Equal(s.T(), s.ExpectedActive, observedActive, "Unexpected number of active connections reported")
	assert.Equal(s.T(), s.ExpectedInactive, observedInactive, "Unexpected number of inactive connections reported")

	// Server side checks

	actualServerEndpoint := networkInfos[0].LocalAddress
	actualClientEndpoint := networkInfos[0].RemoteAddress

	// From server perspective, network connection info only has local port and remote IP
	assert.Equal(s.T(), fmt.Sprintf(":%s", s.ServerPort), actualServerEndpoint)
	assert.Equal(s.T(), s.ClientIP, actualClientEndpoint)

	// client side checks

	// NetworkSignalHandler does not currently report endpoints.
	// See the comment above for the server container endpoint test for more info.
	assert.Equal(s.T(), 0, len(s.Sensor().Endpoints(s.ClientContainer)))

	networkInfos = s.Sensor().Connections(s.ClientContainer)

	actualClientEndpoint = networkInfos[0].LocalAddress
	actualServerEndpoint = networkInfos[0].RemoteAddress
}
