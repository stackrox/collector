package suites

import (
	"fmt"
	"strconv"
	"time"

	"github.com/stretchr/testify/assert"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"
)

type AsyncConnectionTestSuite struct {
	IntegrationTestSuiteBase
	DisableConnectionStatusTracking bool
	BlockConnection                 bool
	ExpectToSeeTheConnection        bool

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
	s.RegisterCleanup("server", "client")

	collectorOptions := collector.StartupOptions{
		Env: map[string]string{
			"ROX_COLLECT_CONNECTION_STATUS": strconv.FormatBool(!s.DisableConnectionStatusTracking),
		},
		Config: map[string]any{
			"turnOffScrape": true,
			// we need a short scrape interval to have it trigger
			// after the connection is closed.
			"scrapeInterval": 2,
		},
	}

	s.StartCollector(false, &collectorOptions)

	image_store := config.Images()

	containerID, err := s.Executor().StartContainer(config.ContainerStartConfig{
		Name:  "server",
		Image: image_store.QaImageByKey("qa-nginx"),
	})
	s.Require().NoError(err)
	s.serverContainer = common.ContainerShortID(containerID)

	s.serverIP, err = s.getIPAddress("server")
	s.Require().NoError(err)

	common.Sleep(5 * time.Second) // TODO use the endpoint declaration

	target := s.serverIP

	if s.BlockConnection {
		target = "10.255.255.1"
	}

	containerID, err = s.Executor().StartContainer(
		config.ContainerStartConfig{
			Name:    "client",
			Image:   image_store.QaImageByKey("qa-alpine-curl"),
			Command: []string{"curl", "--connect-timeout", "5", fmt.Sprintf("http://%s/", target)},
		})
	s.Require().NoError(err)
	s.clientContainer = common.ContainerShortID(containerID)

	common.Sleep(10 * time.Second) // give some time to the connection to fail
}

func (s *AsyncConnectionTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainers("server", "client")
	s.WritePerfResults()
}

func (s *AsyncConnectionTestSuite) TestNetworkFlows() {
	networkInfos := s.Sensor().Connections(s.clientContainer)

	if s.ExpectToSeeTheConnection {
		// expect one connection
		assert.Equal(s.T(), 1, len(networkInfos))
	} else {
		// expect no connections at all
		assert.Equal(s.T(), 0, len(networkInfos))
	}
}
