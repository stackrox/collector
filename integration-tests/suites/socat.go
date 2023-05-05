package suites

import (
	"fmt"
	"regexp"
	"strconv"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stretchr/testify/assert"
)

//
// This test is to guard against behavior seen in Collector where multiple
// process endpoints have incorrect originator process information due to a
// bug in the PID processing for endpoints in the same container.
//
// https://issues.redhat.com/browse/ROX-13560
//
// The test will start two socat processes, with distinct listening ports
// and check that their originators match the appropriate process.
//

type SocatTestSuite struct {
	IntegrationTestSuiteBase
	serverContainer string
}

func (s *SocatTestSuite) SetupSuite() {

	s.metrics = map[string]float64{}
	s.executor = common.NewExecutor()
	s.StartContainerStats()
	s.collector = common.NewCollectorManager(s.executor, s.T().Name())

	s.collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":false,"scrapeInterval":2}`
	s.collector.Env["ROX_PROCESSES_LISTENING_ON_PORT"] = "true"

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)

	err = s.collector.WaitForCollector(60)
	s.Require().NoError(err)

	processImage := common.QaImage("quay.io/rhacs-eng/qa", "socat")

	// the socat container only needs to exist long enough for use to run both
	// socat commands. 300 seconds should be more than long enough.
	containerID, err := s.launchContainer("socat", processImage, "/bin/sh", "-c", "/bin/sleep 300")
	s.Require().NoError(err)

	_, err = s.execContainer("socat", []string{"/bin/sh", "-c", "socat TCP-LISTEN:80,fork STDOUT &"})
	s.Require().NoError(err)

	_, err = s.execContainer("socat", []string{"/bin/sh", "-c", "socat TCP-LISTEN:8080,fork STDOUT &"})
	s.Require().NoError(err)

	s.serverContainer = common.ContainerShortID(containerID)

	time.Sleep(6 * time.Second)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *SocatTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"socat", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("Socat", stats, s.metrics)
}

func (s *SocatTestSuite) TestSocat() {
	processes, err := s.GetProcesses(s.serverContainer)
	s.Require().NoError(err)
	endpoints, err := s.GetEndpoints(s.serverContainer)
	s.Require().NoError(err)

	if !assert.Equal(s.T(), 2, len(endpoints)) {
		// We can't continue if this is not the case, so panic immediately.
		// It indicates an internal issue with this test and the non-deterministic
		// way in which endpoints are reported.
		assert.FailNowf(s.T(), "", "only retrieved %d endpoints (expect 2)", len(endpoints))
	}

	assert.Equal(s.T(), 6, len(processes))

	endpoint80, err := getEndpointByPort(endpoints, 80)
	s.Require().NoError(err)
	endpoint8080, err := getEndpointByPort(endpoints, 8080)
	s.Require().NoError(err)

	process80, err := getProcessByPort(processes, 80)
	s.Require().NoError(err)
	process8080, err := getProcessByPort(processes, 8080)
	s.Require().NoError(err)

	assert.Equal(s.T(), "L4_PROTOCOL_TCP", endpoint80.Protocol)
	assert.Equal(s.T(), endpoint80.Originator.ProcessName, process80.Name)
	assert.Equal(s.T(), endpoint80.Originator.ProcessExecFilePath, process80.ExePath)

	assert.Equal(s.T(), endpoint80.Originator.ProcessArgs, process80.Args)
	assert.Equal(s.T(), 80, endpoint80.Address.Port)

	assert.Equal(s.T(), "L4_PROTOCOL_TCP", endpoint8080.Protocol)
	assert.Equal(s.T(), endpoint8080.Originator.ProcessName, process8080.Name)
	assert.Equal(s.T(), endpoint8080.Originator.ProcessExecFilePath, process8080.ExePath)
	assert.Equal(s.T(), endpoint8080.Originator.ProcessArgs, process8080.Args)
	assert.Equal(s.T(), 8080, endpoint8080.Address.Port)
}

func getEndpointByPort(endpoints []common.EndpointInfo, port int) (*common.EndpointInfo, error) {
	for _, endpoint := range endpoints {
		if endpoint.Address.Port == port {
			return &endpoint, nil
		}
	}

	err := fmt.Errorf("Could not find endpoint with port %d", port)

	return nil, err
}

func getProcessByPort(processes []common.ProcessInfo, port int) (*common.ProcessInfo, error) {
	re := regexp.MustCompile(`:(` + strconv.Itoa(port) + `),`)
	for _, process := range processes {
		portArr := re.FindStringSubmatch(process.Args)
		if len(portArr) == 2 && process.Name != "sh" {
			return &process, nil
		}
	}

	err := fmt.Errorf("Could not find process with port %d", port)

	return nil, err
}
