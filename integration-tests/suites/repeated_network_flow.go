package suites

import (
	"fmt"
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
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

	scheduled_curls_image := common.QaImage("pstauffer/curl", "latest")

	images := []string{
		"nginx:1.14-alpine",
		scheduled_curls_image,
	}

	for _, image := range images {
		err := s.executor.PullImage(image)
		s.Require().NoError(err)
	}

	time.Sleep(10 * time.Second)

	// invokes default nginx
	containerID, err := s.launchContainer("nginx", "nginx:1.14-alpine")
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
	_, err = s.execContainerShellScript("nginx-curl", `
		#!/usr/bin/env sh

		num_meta_iter=$1
		num_iter=$2
		sleep_between_curl_time=$3
		sleep_between_iterations=$4
		url=$5

		i=0
		j=0

		while [ "$i" -lt "$num_meta_iter" ]; do
		    while [ "$j" -lt "$num_iter" ]; do
		        curl "$url"
		        sleep "$sleep_between_curl_time"
		        j=$((j + 1))
		    done
		    sleep "$sleep_between_iterations"
		    i=$((i + 1))
		done`,
		numMetaIter, numIter, sleepBetweenCurlTime, sleepBetweenIterations, serverAddress)

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
	// Server side checks
	assert.Equal(s.T(), s.ExpectedReports, s.ObservedReports)

	val, err := s.Get(s.ServerContainer, networkBucket)
	s.Require().NoError(err)
	actualValues := strings.Split(string(val), "|")

	if len(actualValues) < 2 {
		assert.FailNow(s.T(), "serverContainer networkBucket was missing data. ", "val=\"%s\"", val)
	}
	actualServerEndpoint := actualValues[0]
	actualClientEndpoint := actualValues[1]

	// From server perspective, network connection info only has local port and remote IP
	assert.Equal(s.T(), fmt.Sprintf(":%s", s.ServerPort), actualServerEndpoint)
	assert.Equal(s.T(), s.ClientIP, actualClientEndpoint)

	fmt.Printf("ServerDetails from Bolt: %s %s\n", s.ServerContainer, string(val))
	fmt.Printf("ServerDetails from test: %s %s, Port: %s\n", s.ServerContainer, s.ServerIP, s.ServerPort)

	// client side checks
	val, err = s.Get(s.ClientContainer, networkBucket)
	s.Require().NoError(err)
	actualValues = strings.Split(string(val), "|")

	actualClientEndpoint = actualValues[0]
	actualServerEndpoint = actualValues[1]

	// From client perspective, network connection info has no local endpoint and full remote endpoint
	assert.Empty(s.T(), actualClientEndpoint)
	assert.Equal(s.T(), fmt.Sprintf("%s:%s", s.ServerIP, s.ServerPort), actualServerEndpoint)

	fmt.Printf("ClientDetails from Bolt: %s %s\n", s.ClientContainer, string(val))
	fmt.Printf("ClientDetails from test: %s %s\n", s.ClientContainer, s.ClientIP)
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
