package integrationtests

import (
	"testing"
	"strconv"
	"fmt"
	"time"
	"strings"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/suite"
)

type RepeatedNetworkFlowTestSuite struct {
	//The goal with these integration tests is to make sure we report the correct number of
	//networking events. Sometimes if a connection is made multiple times within a short time
	//called an "afterglow" period, we only want to report the connection once.
	IntegrationTestSuiteBase
	clientContainer        string
	clientIP               string
	serverContainer        string
	serverIP               string
	serverPort             string
	enableAfterglow        bool
	afterglowPeriod        int
	scrapeInterval         int
	numMetaIter            int
	numIter                int
	sleepBetweenCurlTime   int
	sleepBetweenIterations int
	expectedReports        []bool // An array of booleans representing the connection. true is active. fasle is inactive.
	observedReports        []bool
}

//Need to update the comments in these tests
func TestRepeatedNetworkFlow(t *testing.T) {
	// Perform 11 curl commands with a 2 second sleep between each curl command.
	// The scrapeInterval is increased to 4 seconds to reduce the chance that jiter will effect the results.
	// The first server to client connection is recorded as being active.
	// The second through ninth curl commands are ignored, because of afterglow.
	// The last server to client connection is recorded as being inacitve when the afterglow period has expired
	// Thus the reported connections are active, inactive
	repeatedNetworkFlowTestSuite := &RepeatedNetworkFlowTestSuite{
		afterglowPeriod:        10,
		scrapeInterval:         4,
		enableAfterglow:        true,
		numMetaIter:            1,
		numIter:                11,
		sleepBetweenCurlTime:   2,
		sleepBetweenIterations: 1,
		expectedReports:        []bool{true, false},
	}
	suite.Run(t, repeatedNetworkFlowTestSuite)
}

func TestRepeatedNetworkFlowWithZeroAfterglowPeriod(t *testing.T) {
	// Afterglow is disables as the afterglowPeriod is 0
	// All server to client connections are reported.
	repeatedNetworkFlowTestSuite := &RepeatedNetworkFlowTestSuite{
		afterglowPeriod:        0,
		scrapeInterval:         2,
		enableAfterglow:        true,
		numMetaIter:            1,
		numIter:                3,
		sleepBetweenCurlTime:   3,
		sleepBetweenIterations: 1,
		expectedReports:        []bool{false, false, false},
	}
	suite.Run(t, repeatedNetworkFlowTestSuite)
}

func TestRepeatedNetworkFlowThreeCurlsNoAfterglow(t *testing.T) {
	// The afterglow period is set to 0 so this has the same behavior as if afterglow was disabled.
	repeatedNetworkFlowTestSuite := &RepeatedNetworkFlowTestSuite{
		afterglowPeriod:        0,
		scrapeInterval:         4,
		enableAfterglow:        false,
		numMetaIter:            1,
		numIter:                3,
		sleepBetweenCurlTime:   6,
		sleepBetweenIterations: 1,
		expectedReports:        []bool{false, false, false},
	}
	suite.Run(t, repeatedNetworkFlowTestSuite)
}

// Launches collector
// Launches gRPC server in insecure mode
// Launches nginx container
func (s *RepeatedNetworkFlowTestSuite) SetupSuite() {
	s.metrics = map[string]float64{}
	s.executor = NewExecutor()
	s.docker = NewDocker(s.executor)
	s.StartContainerStats()
	s.collector = NewCollectorManager(s.executor, s.T().Name())

	s.collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":true,"scrapeInterval":` + strconv.Itoa(s.scrapeInterval) + `}`
	s.collector.Env["ROX_AFTERGLOW_PERIOD"] = strconv.Itoa(s.afterglowPeriod)
	s.collector.Env["ROX_ENABLE_AFTERGLOW"] = strconv.FormatBool(s.enableAfterglow)

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)

	images := []string{
		"nginx:1.14-alpine",
		"stackrox/qa:collector-schedule-curls",
	}

	for _, image := range images {
		err := s.docker.Pull(image)
		s.Require().NoError(err)
	}

	time.Sleep(10 * time.Second)

	// invokes default nginx
	containerID, err := s.launchContainer("nginx", "nginx:1.14-alpine")
	s.Require().NoError(err)
	s.serverContainer = containerID[0:12]

	// invokes another container
	containerID, err = s.launchContainer("nginx-curl", "stackrox/qa:collector-schedule-curls", "sleep", "300")
	s.Require().NoError(err)
	s.clientContainer = containerID[0:12]

	s.serverIP, err = s.getIPAddress("nginx")
	s.Require().NoError(err)

	s.serverPort, err = s.getPort("nginx")
	s.Require().NoError(err)
	time.Sleep(30 * time.Second)

	serverAddress := fmt.Sprintf("%s:%s", s.serverIP, s.serverPort)

	numMetaIter := strconv.Itoa(s.numMetaIter)
	numIter := strconv.Itoa(s.numIter)
	sleepBetweenCurlTime := strconv.Itoa(s.sleepBetweenCurlTime)
	sleepBetweenIterations := strconv.Itoa(s.sleepBetweenIterations)
	_, err = s.execContainer("nginx-curl", []string{"/usr/bin/schedule-curls.sh", numMetaIter, numIter, sleepBetweenCurlTime, sleepBetweenIterations, serverAddress})

	s.clientIP, err = s.getIPAddress("nginx-curl")
	s.Require().NoError(err)

	totalTime := (s.sleepBetweenCurlTime*s.numIter+s.sleepBetweenIterations)*s.numMetaIter + s.afterglowPeriod + 10
	time.Sleep(time.Duration(totalTime) * time.Second)
	logLines := s.GetLogLines("grpc-server")
	s.observedReports = GetNetworkActivity(logLines, serverAddress)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *RepeatedNetworkFlowTestSuite) TearDownSuite() {
	s.cleanupContainers("nginx", "nginx-curl", "collector")
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("repeated_network_flow", stats, s.metrics)
}

func (s *RepeatedNetworkFlowTestSuite) TestRepeatedNetworkFlow() {
	// Server side checks
	assert.Equal(s.T(), s.expectedReports, s.observedReports)

	val, err := s.Get(s.serverContainer, networkBucket)
	s.Require().NoError(err)
	actualValues := strings.Split(string(val), "|")

	if len(actualValues) < 2 {
		assert.FailNow(s.T(), "serverContainer networkBucket was missing data. ", "val=\"%s\"", val)
	}
	actualServerEndpoint := actualValues[0]
	actualClientEndpoint := actualValues[1]

	// From server perspective, network connection info only has local port and remote IP
	assert.Equal(s.T(), fmt.Sprintf(":%s", s.serverPort), actualServerEndpoint)
	assert.Equal(s.T(), s.clientIP, actualClientEndpoint)

	fmt.Printf("ServerDetails from Bolt: %s %s\n", s.serverContainer, string(val))
	fmt.Printf("ServerDetails from test: %s %s, Port: %s\n", s.serverContainer, s.serverIP, s.serverPort)

	// client side checks
	val, err = s.Get(s.clientContainer, networkBucket)
	s.Require().NoError(err)
	actualValues = strings.Split(string(val), "|")

	actualClientEndpoint = actualValues[0]
	actualServerEndpoint = actualValues[1]

	// From client perspective, network connection info has no local endpoint and full remote endpoint
	assert.Empty(s.T(), actualClientEndpoint)
	assert.Equal(s.T(), fmt.Sprintf("%s:%s", s.serverIP, s.serverPort), actualServerEndpoint)

	fmt.Printf("ClientDetails from Bolt: %s %s\n", s.clientContainer, string(val))
	fmt.Printf("ClientDetails from test: %s %s\n", s.clientContainer, s.clientIP)
}