package suites

import (
	"fmt"
	"time"

	"github.com/stretchr/testify/assert"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/config"
)

type AsyncConnectionTestSuite struct {
	IntegrationTestSuiteBase
	SetReportConnectionAttemptsEnvVar bool
	BlockConnection                   bool
	ExpectToSeeTheConnection          bool

	clientContainer string
	serverContainer string
	serverIP        string
}

/*
 * This suite has 2 main cases:
 * - BlockConnection=false: verify that a successful asynchronous connection gets reported
 *     We provide curl-test with the IP of a TCP server to isntruct it to connect to this server.
 * - BlockConnection=true : verify that a failed aynchronous connection is not
 *     No parameter to curl-test, and in this case, it simulates a firewall rule by connecting to
 *     a dummy address.
 */
func (s *AsyncConnectionTestSuite) SetupSuite() {

	s.metrics = map[string]float64{}
	s.executor = common.NewExecutor()
	s.StartContainerStats()
	s.collector = common.NewCollectorManager(s.executor, s.T().Name())

	// we need a short scrape interval to have it trigger after the connection is closed.
	s.collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":false,"scrapeInterval":2}`

	if s.SetReportConnectionAttemptsEnvVar {
		s.collector.Env["ROX_COLLECTOR_REPORT_CONNECTION_ATTEMPTS"] = "true"
	}

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)
	time.Sleep(30 * time.Second) // TODO use readiness

	image_store := config.Images()

	containerID, err := s.launchContainer("server", image_store.ImageByKey("nginx"))
	s.Require().NoError(err)
	s.serverContainer = common.ContainerShortID(containerID)

	s.serverIP, err = s.getIPAddress("server")
	s.Require().NoError(err)

	time.Sleep(5 * time.Second) // TODO use the endpoint declaration

	target := s.serverIP

	if s.BlockConnection {
		target = "10.255.255.1"
	}

	containerID, err = s.launchContainer("client", image_store.QaImageByKey("curl"), "curl", "--connect-timeout", "5", fmt.Sprintf("http://%s/", target))

	s.Require().NoError(err)
	s.clientContainer = common.ContainerShortID(containerID)

	time.Sleep(10 * time.Second) // give some time to the connection to fail

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *AsyncConnectionTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"server", "client"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("AsyncConnection", stats, s.metrics)
}

func (s *AsyncConnectionTestSuite) TestNetworkFlows() {
	networkInfos, err := s.GetNetworks(s.clientContainer)

	if s.ExpectToSeeTheConnection {
		// expect one connection
		s.Require().NoError(err)

		assert.Equal(s.T(), 1, len(networkInfos))
	} else {
		// expect no connections at all
		s.Require().Error(err)
	}
}
