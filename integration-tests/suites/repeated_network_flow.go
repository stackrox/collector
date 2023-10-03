package suites

import (
	"fmt"
	"strconv"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/config"
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
	defer s.RecoverSetup("nginx", "nginx-curl")
	s.StartContainerStats()

	s.Collector().Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":true,"scrapeInterval":` + strconv.Itoa(s.ScrapeInterval) + `}`
	s.Collector().Env["ROX_AFTERGLOW_PERIOD"] = strconv.Itoa(s.AfterglowPeriod)
	s.Collector().Env["ROX_ENABLE_AFTERGLOW"] = strconv.FormatBool(s.EnableAfterglow)

	s.StartCollector(false)

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

	time.Sleep(10 * time.Second)

	// invokes default nginx
	containerID, err := s.launchContainer("nginx", image_store.ImageByKey("nginx"))
	s.Require().NoError(err)
	s.ServerContainer = containerID[0:12]

	// invokes another container
	containerID, err = s.launchContainer("nginx-curl", scheduled_curls_image, "sleep", "300")
	s.Require().NoError(err)
	s.ClientContainer = containerID[0:12]

	s.ServerIP, err = s.getIPAddress("nginx")
	s.Require().NoError(err)

	s.ServerPort, err = s.getPort("nginx")
	s.Require().NoError(err)
	time.Sleep(30 * time.Second)

	serverAddress := fmt.Sprintf("%s:%s", s.ServerIP, s.ServerPort)

	numMetaIter := strconv.Itoa(s.NumMetaIter)
	numIter := strconv.Itoa(s.NumIter)
	sleepBetweenCurlTime := strconv.Itoa(s.SleepBetweenCurlTime)
	sleepBetweenIterations := strconv.Itoa(s.SleepBetweenIterations)
	_, err = s.execContainer("nginx-curl", []string{"/usr/bin/schedule-curls.sh", numMetaIter, numIter, sleepBetweenCurlTime, sleepBetweenIterations, serverAddress})

	s.ClientIP, err = s.getIPAddress("nginx-curl")
	s.Require().NoError(err)

	totalTime := (s.SleepBetweenCurlTime*s.NumIter+s.SleepBetweenIterations)*s.NumMetaIter + s.AfterglowPeriod + 10
	time.Sleep(time.Duration(totalTime) * time.Second)
}

func (s *RepeatedNetworkFlowTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainer([]string{"nginx", "nginx-curl"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("repeated_network_flow", stats, s.metrics)
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
