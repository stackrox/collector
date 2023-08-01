package suites

import (
	"fmt"
	"regexp"
	"strconv"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
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
	ExpectedReports        []bool // An array of booleans representing the connection. true is active. fasle is inactive.
	ObservedReports        []bool
}

// Launches collector
// Launches gRPC server in insecure mode
// Launches nginx container
func (s *RepeatedNetworkFlowTestSuite) SetupSuite() {
	s.metrics = map[string]float64{}
	s.executor = common.NewExecutor()
	s.StartContainerStats()
	s.collector = common.NewCollectorManager(s.executor, s.T().Name())

	s.collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":true,"scrapeInterval":` + strconv.Itoa(s.ScrapeInterval) + `}`
	s.collector.Env["ROX_AFTERGLOW_PERIOD"] = strconv.Itoa(s.AfterglowPeriod)
	s.collector.Env["ROX_ENABLE_AFTERGLOW"] = strconv.FormatBool(s.EnableAfterglow)

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)

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
	logLines := s.GetLogLines("grpc-server")
	s.ObservedReports = GetNetworkActivity(logLines, serverAddress)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *RepeatedNetworkFlowTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"nginx", "nginx-curl", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("repeated_network_flow", stats, s.metrics)
}

func (s *RepeatedNetworkFlowTestSuite) TestRepeatedNetworkFlow() {
	assert.Equal(s.T(), s.ExpectedReports, s.ObservedReports)

	// Server side checks

	networkInfos, err := s.GetNetworks(s.ServerContainer)
	s.Require().NoError(err)

	actualServerEndpoint := networkInfos[0].LocalAddress
	actualClientEndpoint := networkInfos[0].RemoteAddress

	// From server perspective, network connection info only has local port and remote IP
	assert.Equal(s.T(), fmt.Sprintf(":%s", s.ServerPort), actualServerEndpoint)
	assert.Equal(s.T(), s.ClientIP, actualClientEndpoint)

	fmt.Printf("ServerDetails from Bolt: %s %+v\n", s.ServerContainer, networkInfos[0])
	fmt.Printf("ServerDetails from test: %s %s, Port: %s\n", s.ServerContainer, s.ServerIP, s.ServerPort)

	// client side checks

	// NetworkSignalHandler does not currently report endpoints.
	// See the comment above for the server container endpoint test for more info.
	_, err = s.GetEndpoints(s.ClientContainer)
	s.Require().Error(err)

	networkInfos, err = s.GetNetworks(s.ClientContainer)
	s.Require().NoError(err)

	actualClientEndpoint = networkInfos[0].LocalAddress
	actualServerEndpoint = networkInfos[0].RemoteAddress
}

func GetNetworkActivity(lines []string, serverAddress string) []bool {
	var networkActivity []bool
	inactivePattern := "^Network.*" + serverAddress + ".*Z$"
	activePattern := "^Network.*" + serverAddress + ".*nil Timestamp.$"
	for _, line := range lines {
		activeMatch, _ := regexp.MatchString(activePattern, line)
		inactiveMatch, _ := regexp.MatchString(inactivePattern, line)
		if activeMatch {
			networkActivity = append(networkActivity, true)
		} else if inactiveMatch {
			networkActivity = append(networkActivity, false)
		}

	}
	return networkActivity
}
