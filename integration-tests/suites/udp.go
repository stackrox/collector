package suites

import (
	"strconv"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stretchr/testify/assert"
)

type UdpTestSuite struct {
	IntegrationTestSuiteBase
	ListenContainer string
	SendtoContainer string
	CollectUdp      bool
}

// This test checks if we are reporting UDP endpoints correctly. It launches collector and two
// socat containers. One listens for UDP messages and the other is set up to send UDP messages.
// We make sure that listening UDP endpoint is seen and that the sending UDP endpoint is not.

func (s *UdpTestSuite) SetupSuite() {

	s.metrics = map[string]float64{}
	s.executor = common.NewExecutor()
	s.StartContainerStats()
	s.collector = common.NewCollectorManager(s.executor, s.T().Name())

	s.collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":false,"scrapeInterval":2}`
	s.collector.Env["ROX_PROCESSES_LISTENING_ON_PORT"] = "true"
	s.collector.Env["ROX_COLLECT_UDP_LISTENING_ENDPOINTS"] = strconv.FormatBool(s.CollectUdp)

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)
	time.Sleep(30 * time.Second)

	processImage := common.QaImage("quay.io/rhacs-eng/qa", "socat")

	containerListenID, err := s.launchContainer("socat-listen", processImage, "/bin/sh", "-c", "/bin/sleep 3000")
	s.Require().NoError(err)

	containerSendtoID, err := s.launchContainer("socat-sendto", processImage, "/bin/sh", "-c", "/bin/sleep 3000")
	s.Require().NoError(err)

	_, err = s.execContainer("socat-listen", []string{"/bin/sh", "-c", "socat -d -d -v UDP-RECVFROM:80,fork - &> /socat-log.txt &"})
	s.Require().NoError(err)

	listenIP, err := s.getIPAddress("socat-listen")
	s.Require().NoError(err)

	_, err = s.execContainer("socat-sendto", []string{"/bin/sh", "-c", "echo Hello | socat -d -d -v UDP-SENDTO:" + listenIP + ":80 STDIN &> /socat-log.txt &"})
	s.Require().NoError(err)

	s.ListenContainer = common.ContainerShortID(containerListenID)
	s.SendtoContainer = common.ContainerShortID(containerSendtoID)

	time.Sleep(6 * time.Second)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *UdpTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"socat-listen", "socat-sendto", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("UDP", stats, s.metrics)
}

func (s *UdpTestSuite) TestUdp() {
	processes, err := s.GetProcesses(s.ListenContainer)
	s.Require().NoError(err)

	assert.Equal(s.T(), 4, len(processes))

	sendtoProcesses, err := s.GetProcesses(s.SendtoContainer)
	s.Require().NoError(err)
	sendtoEndpoints, err := s.GetEndpoints(s.SendtoContainer)
	s.Require().Error(err)

	// There should not be any UDP listening endpoints in the container sending UDP messages
	assert.Equal(s.T(), 0, len(sendtoEndpoints))
	assert.Equal(s.T(), 4, len(sendtoProcesses))

	if s.CollectUdp {
		// If UDP endpoints are collected we should expect to see it
		endpoints, err := s.GetEndpoints(s.ListenContainer)
		s.Require().NoError(err)

		if !assert.Equal(s.T(), 1, len(endpoints)) {
			// We can't continue if this is not the case, so panic immediately.
			// It indicates an internal issue with this test and the non-deterministic
			// way in which endpoints are reported.
			assert.FailNowf(s.T(), "", "retrieved %d endpoints (expect 1)", len(endpoints))
		}

		assert.Equal(s.T(), "L4_PROTOCOL_UDP", endpoints[0].Protocol)
		assert.Equal(s.T(), endpoints[0].Originator.ProcessName, processes[3].Name)
		assert.Equal(s.T(), endpoints[0].Originator.ProcessExecFilePath, processes[3].ExePath)
		assert.Equal(s.T(), endpoints[0].Originator.ProcessArgs, processes[3].Args)
		assert.Equal(s.T(), 80, endpoints[0].Address.Port)

	} else {
		// If collecting UDP is disabled trying to get the endpoints from the bucket should result in an error
		_, err := s.GetEndpoints(s.ListenContainer)
		s.Require().Error(err)
	}
}
