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

type RepeatedNetworkFlowTestSuite struct {
	//The goal with these integration tests is to make sure we report the correct number of
	//networking events. Sometimes if a connection is made multiple times within a short time
	//called an "afterglow" period, we only want to report the connection once.
	IntegrationTestSuiteBase
	ClientContainer        string
	ClientIP               string
	ServerContainer        string
	ServerIP               string
	ServerPort             string
	EnableAfterglow        bool
	AfterglowPeriod        int
	ScrapeInterval         int
	NumMetaIter            int
	NumIter                int
	SleepBetweenCurlTime   int
	SleepBetweenIterations int
	ExpectedActive         int // number of active connections expected
	ExpectedInactive       int // number of inactive connections expected
}

// Launches collector
// Launches gRPC server in insecure mode
// Launches nginx container
func (s *RepeatedNetworkFlowTestSuite) SetupSuite() {
	s.RegisterCleanup("nginx", "nginx-curl")

	collectorOptions := collector.StartupOptions{
		Config: map[string]any{
			// turnOffScrape will be true, but the scrapeInterval
			// also controls the reporting interval for network events
			"scrapeInterval": s.ScrapeInterval,
		},
		Env: map[string]string{
			"ROX_AFTERGLOW_PERIOD": strconv.Itoa(s.AfterglowPeriod),
			"ROX_ENABLE_AFTERGLOW": strconv.FormatBool(s.EnableAfterglow),
		},
	}

	s.StartCollector(false, &collectorOptions)

	image_store := config.Images()
	scheduled_curls_image := image_store.QaImageByKey("qa-schedule-curls")

	images := []string{
		image_store.ImageByKey("nginx"),
		scheduled_curls_image,
	}

	for _, image := range images {
		err := s.executor.PullImage(image)
		s.Require().NoError(err)
	}

	// invokes default nginx
	containerID, err := s.Executor().StartContainer(config.ContainerStartConfig{
		Name:  "nginx",
		Image: image_store.ImageByKey("nginx"),
		Ports: []uint16{80},
	})
	s.Require().NoError(err)
	s.ServerContainer = containerID[0:12]

	// invokes another container
	containerID, err = s.Executor().StartContainer(config.ContainerStartConfig{Name: "nginx-curl", Image: scheduled_curls_image, Command: []string{"sleep", "300"}})
	s.Require().NoError(err)
	s.ClientContainer = containerID[0:12]

	s.ServerIP, err = s.getIPAddress("nginx")
	s.Require().NoError(err)

	s.ServerPort, err = s.getPort("nginx")
	s.Require().NoError(err)

	serverAddress := fmt.Sprintf("%s:%s", s.ServerIP, s.ServerPort)

	numMetaIter := strconv.Itoa(s.NumMetaIter)
	numIter := strconv.Itoa(s.NumIter)
	sleepBetweenCurlTime := strconv.Itoa(s.SleepBetweenCurlTime)
	sleepBetweenIterations := strconv.Itoa(s.SleepBetweenIterations)
	_, err = s.execContainer("nginx-curl", []string{"/usr/bin/schedule-curls.sh", numMetaIter, numIter, sleepBetweenCurlTime, sleepBetweenIterations, serverAddress}, false)

	s.ClientIP, err = s.getIPAddress("nginx-curl")
	s.Require().NoError(err)

	totalTime := (s.SleepBetweenCurlTime*s.NumIter+s.SleepBetweenIterations)*s.NumMetaIter + s.AfterglowPeriod + 10
	common.Sleep(time.Duration(totalTime) * time.Second)
}

func (s *RepeatedNetworkFlowTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainers("nginx", "nginx-curl")
	s.WritePerfResults()
}

func (s *RepeatedNetworkFlowTestSuite) TestRepeatedNetworkFlow() {
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
