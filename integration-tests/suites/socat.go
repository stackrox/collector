package suites

import (
	"fmt"
	"regexp"
	"strconv"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/config"
	"github.com/stackrox/collector/integration-tests/suites/types"
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
	defer s.RecoverSetup("socat")
	s.StartContainerStats()

	collectorOptions := common.CollectorStartupOptions{
		Config: map[string]any{
			"turnOffScrape": false,
		},
		Env: map[string]string{
			"ROX_PROCESSES_LISTENING_ON_PORT": "true",
		},
	}

	s.StartCollector(false, &collectorOptions)

	processImage := config.Images().QaImageByKey("qa-socat")

	// the socat container only needs to exist long enough for use to run both
	// socat commands. 300 seconds should be more than long enough.
	containerID, err := s.launchContainer("socat", processImage, "TCP-LISTEN:80,fork", "STDOUT")
	s.Require().NoError(err)

	_, err = s.execContainer("socat", []string{"/bin/sh", "-c", "socat TCP-LISTEN:8080,fork STDOUT &"})
	s.Require().NoError(err)

	s.serverContainer = common.ContainerShortID(containerID)

	time.Sleep(6 * time.Second)
}

func (s *SocatTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainers("socat")
	s.WritePerfResults()
}

func (s *SocatTestSuite) TestSocat() {
	processes := s.Sensor().ExpectProcessesN(s.T(), s.serverContainer, 10*time.Second, 3)
	endpoints := s.Sensor().ExpectEndpointsN(s.T(), s.serverContainer, 10*time.Second, 2)

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

func getEndpointByPort(endpoints []types.EndpointInfo, port int) (*types.EndpointInfo, error) {
	for _, endpoint := range endpoints {
		if endpoint.Address.Port == port {
			return &endpoint, nil
		}
	}

	err := fmt.Errorf("Could not find endpoint with port %d", port)

	return nil, err
}

func getProcessByPort(processes []types.ProcessInfo, port int) (*types.ProcessInfo, error) {
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
