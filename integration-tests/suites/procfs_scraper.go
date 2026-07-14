package suites

import (
	"strconv"
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/types"
)

// ProcfsScraperTestSuite verifies that collector discovers pre-existing
// listening sockets by scraping /proc at startup, before the BPF event
// stream is active. This is critical because connections and listeners
// established before collector starts would otherwise be invisible.
// The key ordering constraint: nginx must be started *before* collector
// so that its listening socket exists in procfs at scrape time but is
// never seen via a BPF bind/listen event.
type ProcfsScraperTestSuite struct {
	IntegrationTestSuiteBase
	ServerContainer             string
	TurnOffScrape               bool
	RoxProcessesListeningOnPort bool
	Expected                    []types.EndpointInfo
}

// SetupSuite starts nginx *before* collector — the reverse of other
// suites. This ordering is deliberate: the listening socket must already
// exist in /proc when collector's procfs scraper runs at startup, but
// must not trigger a BPF bind/listen event (which would be handled by
// NetworkSignalHandler instead, defeating the purpose of the test).
func (s *ProcfsScraperTestSuite) SetupSuite() {
	s.RegisterCleanup("nginx")

	collectorOptions := collector.StartupOptions{
		Env: map[string]string{
			"ROX_PROCESSES_LISTENING_ON_PORT": strconv.FormatBool(s.RoxProcessesListeningOnPort),
		},
		Config: map[string]any{
			"turnOffScrape": s.TurnOffScrape,
		},
	}

	s.launchNginx()

	s.StartCollector(false, &collectorOptions)

	s.cleanupContainers("nginx")
}

func (s *ProcfsScraperTestSuite) launchNginx() {
	image := config.Images().QaImageByKey("qa-nginx")

	err := s.Executor().PullImage(image)
	s.Require().NoError(err)

	// invokes default nginx
	containerID, err := s.Executor().StartContainer(config.ContainerStartConfig{
		Name:  "nginx",
		Image: image,
	})

	s.Require().NoError(err)
	s.ServerContainer = common.ContainerShortID(containerID)
}

func (s *ProcfsScraperTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainers("nginx")
	s.WritePerfResults()
}

func (s *ProcfsScraperTestSuite) TestProcfsScraper() {
	if len(s.Expected) == 0 {
		// if we expect no endpoints, this Expect function is more precise
		s.Sensor().ExpectEndpointsN(s.T(), s.ServerContainer, 10*time.Second, 0)
	} else {
		s.Sensor().ExpectEndpoints(s.T(), s.ServerContainer, 10*time.Second, s.Expected...)
	}
}
