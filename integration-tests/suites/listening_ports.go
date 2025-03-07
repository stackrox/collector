package suites

import (
	"fmt"
	"io"
	"net/http"
	"sort"
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/log"
	"github.com/stackrox/collector/integration-tests/pkg/types"
	"github.com/stretchr/testify/assert"
)

type ProcessListeningOnPortTestSuite struct {
	IntegrationTestSuiteBase
	serverContainer string
	serverURL       string
}

func (s *ProcessListeningOnPortTestSuite) SetupSuite() {
	s.RegisterCleanup("process-ports")
	s.StartContainerStats()

	collectorOptions := collector.StartupOptions{
		Config: map[string]any{
			"turnOffScrape":  false,
			"scrapeInterval": 1,
		},
		Env: map[string]string{
			"ROX_PROCESSES_LISTENING_ON_PORT": "true",
		},
	}

	s.StartCollector(false, &collectorOptions)

	processImage := getProcessListeningOnPortsImage()

	serverName := "process-ports"
	err := s.executor.PullImage(processImage)
	s.Require().NoError(err)
	containerID, err := s.Executor().StartContainer(
		config.ContainerStartConfig{
			Name:  serverName,
			Image: processImage,
			Ports: []uint16{5000},
		})
	s.Require().NoError(err)

	s.serverContainer = common.ContainerShortID(containerID)

	ip, err := s.getIPAddress(serverName)
	s.Require().NoError(err)

	port, err := s.getPort(serverName)
	s.Require().NoError(err)

	s.serverURL = fmt.Sprintf("http://%s:%s", ip, port)

	// Wait 5 seconds for the plop service to start
	common.Sleep(5 * time.Second)

	log.Info("Opening ports...")
	s.openPort(8081)
	s.openPort(9091)

	common.Sleep(6 * time.Second)

	log.Info("Closing ports...")
	s.closePort(8081)
	s.closePort(9091)
}

func (s *ProcessListeningOnPortTestSuite) TearDownSuite() {
	s.StopCollector()
	s.cleanupContainers("process-ports")
	s.WritePerfResults()
}

func (s *ProcessListeningOnPortTestSuite) TestProcessListeningOnPort() {
	processes := s.Sensor().ExpectProcessesN(s.T(), s.serverContainer, 5*time.Second, 1)
	endpoints := s.Sensor().ExpectEndpointsN(s.T(), s.serverContainer, 5*time.Second, 5)

	// sort by name to ensure processes[0] is the plop process (the other
	// is the shell)
	// All of these asserts check against the processes information of that program.
	sort.Slice(processes, func(i, j int) bool {
		return processes[i].Name < processes[j].Name
	})

	process := processes[0]

	possiblePorts := []int{8081, 9091, 5000}

	// First verify that all endpoints have the expected metadata, that
	// they are the correct protocol and come from the expected Originator
	// process.
	for _, endpoint := range endpoints {
		assert.Equal(s.T(), "L4_PROTOCOL_TCP", endpoint.Protocol)

		assert.Equal(s.T(), endpoint.Originator.ProcessName, process.Name)
		assert.Equal(s.T(), endpoint.Originator.ProcessExecFilePath, process.ExePath)
		assert.Equal(s.T(), endpoint.Originator.ProcessArgs, process.Args)

		// assert that we haven't got any unexpected ports - further checking
		// of this data will occur subsequently
		assert.Contains(s.T(), possiblePorts, endpoint.Address.Port)
	}

	// We can't guarantee the order in which collector reports the endpoints.
	// Check that we have precisely two pairs of endpoints, for opening
	// and closing the port. A closed port will have a populated CloseTimestamp

	endpoints8081 := make([]types.EndpointInfo, 0)
	endpoints9091 := make([]types.EndpointInfo, 0)
	for _, endpoint := range endpoints {
		if endpoint.Address.Port == 8081 {
			endpoints8081 = append(endpoints8081, endpoint)
		} else if endpoint.Address.Port == 9091 {
			endpoints9091 = append(endpoints9091, endpoint)
		}
		// other ports cannot exist at this point due to previous assertions
	}

	// This helper simplifies the assertions a fair bit, by checking that
	// the recorded endpoints have an open event (CloseTimestamp == nil) and
	// a close event (CloseTimestamp != nil) and not two close events or two open
	// events.
	//
	// It is also agnostic to the order in which the events are reported.
	hasOpenAndClose := func(infos []types.EndpointInfo) bool {
		if !assert.Len(s.T(), infos, 2) {
			return false
		}
		return infos[0].CloseTimestamp != infos[1].CloseTimestamp &&
			(infos[0].CloseTimestamp == types.NilTimestamp || infos[1].CloseTimestamp == types.NilTimestamp)
	}

	assert.True(s.T(), hasOpenAndClose(endpoints8081), "Did not capture open and close events for port 8081")
	assert.True(s.T(), hasOpenAndClose(endpoints9091), "Did not capture open and close events for port 9091")
}

func getProcessListeningOnPortsImage() string {
	return config.Images().QaImageByKey("qa-plop")
}

func (s *ProcessListeningOnPortTestSuite) openPort(port uint16) {
	res, err := http.Get(fmt.Sprintf("%s/open/%d", s.serverURL, port))
	s.Require().NoError(err)
	s.assertResponse(res, "openPort")
}

func (s *ProcessListeningOnPortTestSuite) closePort(port uint16) {
	res, err := http.Get(fmt.Sprintf("%s/close/%d", s.serverURL, port))
	s.Require().NoError(err)
	s.assertResponse(res, "closePort")
}

func (s *ProcessListeningOnPortTestSuite) assertResponse(resp *http.Response, caller string) {
	if resp.StatusCode == 200 {
		return
	}

	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		log.Error("Failed to read response body: %s", err)
		body = []byte("<failed to read body>")
	}

	msg := fmt.Sprintf("%s failed", caller)
	s.Require().FailNowf(msg, "%q", string(body))
}
