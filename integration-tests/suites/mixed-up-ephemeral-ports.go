package suites

import (
	"fmt"
	"strings"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stretchr/testify/assert"
)

type MixedUpEphemeralPortsTestSuite struct {
	IntegrationTestSuiteBase
	serverContainer  string
	clientContainer  string
	serverContainer2 string
	clientContainer2 string
	serverContainer3 string
	clientContainer3 string
}

func (s *MixedUpEphemeralPortsTestSuite) SetupSuite() {

	s.metrics = map[string]float64{}
	s.executor = common.NewExecutor()
	s.StartContainerStats()
	s.collector = common.NewCollectorManager(s.executor, s.T().Name())

	s.collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":false,"scrapeInterval":2}`
	s.collector.Env["ROX_PROCESSES_LISTENING_ON_PORT"] = "true"
	s.collector.Env["ROX_ENABLE_AFTERGLOW"] = "0"

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)
	time.Sleep(30 * time.Second)

	socatImage := common.QaImage("quay.io/rhacs-eng/qa", "socat")

	// the socat container only needs to exist long enough for use to run both
	// socat commands. 300 seconds should be more than long enough.
	serverContainerID, err := s.launchContainer("socat-listen", socatImage, "/bin/sh", "-c", "/bin/sleep 300")
	s.Require().NoError(err)
	time.Sleep(1 * time.Second)

	clientContainerID, err := s.launchContainer("socat-send", socatImage, "/bin/sh", "-c", "/bin/sleep 300")
	s.Require().NoError(err)
	time.Sleep(1 * time.Second)

	serverContainerID2, err := s.launchContainer("socat-listen-2", socatImage, "/bin/sh", "-c", "/bin/sleep 300")
	s.Require().NoError(err)
	time.Sleep(1 * time.Second)

	clientContainerID2, err := s.launchContainer("socat-send-2", socatImage, "/bin/sh", "-c", "/bin/sleep 300")
	s.Require().NoError(err)
	time.Sleep(1 * time.Second)

	serverContainerID3, err := s.launchContainer("socat-listen-3", socatImage, "/bin/sh", "-c", "/bin/sleep 300")
	s.Require().NoError(err)
	time.Sleep(1 * time.Second)

	clientContainerID3, err := s.launchContainer("socat-send-3", socatImage, "/bin/sh", "-c", "/bin/sleep 300")
	fmt.Println("clientContainerID3= " + clientContainerID3)
	s.Require().NoError(err)
	time.Sleep(1 * time.Second)

	time.Sleep(3 * time.Second)

	listenIP, err := s.getIPAddress("socat-listen")
	s.Require().NoError(err)

	listenIP2, err := s.getIPAddress("socat-listen-2")
	s.Require().NoError(err)

	listenIP3, err := s.getIPAddress("socat-listen-3")
	s.Require().NoError(err)

	// Server listens on a port in the ephemeral ports range
	_, err = s.execContainer("socat-listen", []string{"/bin/sh", "-c", "socat TCP4-LISTEN:40000,reuseaddr,fork - &"})
	s.Require().NoError(err)

	// Server listens on another port not in the ephemeral ports range
	_, err = s.execContainer("socat-listen-2", []string{"/bin/sh", "-c", "socat TCP4-LISTEN:40,reuseaddr,fork - &"})

	// Server listens on a port in the ephemeral ports range
	_, err = s.execContainer("socat-listen-3", []string{"/bin/sh", "-c", "socat TCP4-LISTEN:50000,reuseaddr,fork - &"})
	s.Require().NoError(err)

	time.Sleep(6 * time.Second)

	// Client uses a port not in the ephemeral ports range as an ephemeral port
	_, err = s.execContainer("socat-send", []string{"/bin/sh", "-c", "echo hello | socat - TCP4:" + listenIP + ":40000,sourceport=10000"})
	s.Require().NoError(err)

	// Client uses an ephemeral port
	_, err = s.execContainer("socat-send-2", []string{"/bin/sh", "-c", "echo hello | socat - TCP4:" + listenIP2 + ":40"})

	// Client uses a port not in the ephemeral ports range as an ephemeral port and the connection is kept open
	_, err = s.execContainer("socat-send-3", []string{"/bin/sh", "-c", "tail -f /dev/null | socat - TCP4:" + listenIP3 + ":50000,sourceport=20000 &"})
	s.Require().NoError(err)

	s.serverContainer = common.ContainerShortID(serverContainerID)
	s.clientContainer = common.ContainerShortID(clientContainerID)

	s.serverContainer2 = common.ContainerShortID(serverContainerID2)
	s.clientContainer2 = common.ContainerShortID(clientContainerID2)

	s.serverContainer3 = common.ContainerShortID(serverContainerID3)
	s.clientContainer3 = common.ContainerShortID(clientContainerID3)
	fmt.Println("s.clientContainer3= " + s.clientContainer3)

	time.Sleep(6 * time.Second)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *MixedUpEphemeralPortsTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"socat-listen", "socat-send", "socat-listen-2", "socat-send-2", "socat-listen-3", "socat-send-3", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("Socat", stats, s.metrics)
}

func (s *MixedUpEphemeralPortsTestSuite) TestMixedUpEphemeralPorts() {
	serverEndpoints, err := s.GetEndpoints(s.serverContainer)
	s.Require().NoError(err)

	assert.Equal(s.T(), 1, len(serverEndpoints))

	_, err = s.GetEndpoints(s.clientContainer)
	s.Require().Error(err)

	// client side checks
	val, err := s.Get(s.clientContainer, networkBucket)
	s.Require().NoError(err)
	actualValues := strings.Split(string(val), "|")
	assert.Equal(s.T(), "ROLE_CLIENT", actualValues[2])

	fmt.Println(actualValues)

	_, err = s.GetEndpoints(s.clientContainer)
	s.Require().Error(err)

	// server side checks
	val, err = s.Get(s.serverContainer, networkBucket)
	s.Require().NoError(err)
	actualValues = strings.Split(string(val), "|")
	assert.Equal(s.T(), "ROLE_SERVER", actualValues[2])

	// More normal connection

	serverEndpoints2, err := s.GetEndpoints(s.serverContainer2)
	s.Require().NoError(err)

	assert.Equal(s.T(), 1, len(serverEndpoints2))

	_, err = s.GetEndpoints(s.clientContainer2)
	s.Require().Error(err)

	// client side checks
	val, err = s.Get(s.clientContainer2, networkBucket)
	s.Require().NoError(err)
	actualValues = strings.Split(string(val), "|")
	assert.Equal(s.T(), "ROLE_CLIENT", actualValues[2])

	fmt.Println(actualValues)

	// server side checks
	val, err = s.Get(s.serverContainer2, networkBucket)
	s.Require().NoError(err)
	actualValues = strings.Split(string(val), "|")
	assert.Equal(s.T(), "ROLE_SERVER", actualValues[2])

	// Switched ports with open connection

	serverEndpoints3, err := s.GetEndpoints(s.serverContainer3)
	s.Require().NoError(err)

	assert.Equal(s.T(), 1, len(serverEndpoints3))

	_, err = s.GetEndpoints(s.clientContainer3)
	s.Require().Error(err)

	// client side checks
	val, err = s.Get(s.clientContainer3, networkBucket)
	s.Require().NoError(err)
	actualValues = strings.Split(string(val), "|")
	fmt.Println(s.clientContainer3)
	fmt.Println("Client 3")
	fmt.Println(val)
	fmt.Println(actualValues)
	assert.Equal(s.T(), "ROLE_CLIENT", actualValues[2])

	// server side checks
	val, err = s.Get(s.serverContainer3, networkBucket)
	s.Require().NoError(err)
	actualValues = strings.Split(string(val), "|")
	fmt.Println("Server 3")
	fmt.Println(actualValues)
	assert.Equal(s.T(), "ROLE_SERVER", actualValues[2])
}
