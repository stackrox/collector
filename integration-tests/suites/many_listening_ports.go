package suites

import (
	"regexp"
	"strconv"
	"time"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stretchr/testify/assert"
)

type ManyProcessesListeningOnPortsTestSuite struct {
	IntegrationTestSuiteBase
	NumPorts int
	serverContainer string
}

func (s *ManyProcessesListeningOnPortsTestSuite) SetupSuite() {

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
	time.Sleep(30 * time.Second)

	processImage := common.QaImage("quay.io/rhacs-eng/qa", "socat")

	//containerID, err := s.launchContainer("socat", "-e", "PORT1=80", "-e", "PORT2=8080", processImage, "/bin/sh", "-c", "socat TCP-LISTEN:$PORT1,fork STDOUT & socat TCP-LISTEN:$PORT2,fork STDOUT")
	//containerID, err := s.launchContainer("socat", processImage, "/bin/sh", "-c", "sleep 10000")
	containerID, err := s.launchContainer("socat", processImage, "sleep", "10000")

	_, err = s.execContainer("socat", []string{"/bin/sh", "-c", "for port in $(seq " + strconv.Itoa(s.NumPorts) + "); do socat TCP-LISTEN:${port},fork STDOUT & done"})

	s.Require().NoError(err)
	s.serverContainer = common.ContainerShortID(containerID)

	time.Sleep(6 * time.Second)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *ManyProcessesListeningOnPortsTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"socat", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("Socat", stats, s.metrics)
}

func (s *ManyProcessesListeningOnPortsTestSuite) TestManyProcessesListeningOnPorts() {
	processes, err := s.GetProcesses(s.serverContainer)
	s.Require().NoError(err)
	endpoints, err := s.GetEndpoints(s.serverContainer)
	s.Require().NoError(err)

	assert.Equal(s.T(), s.NumPorts, len(endpoints))

	assert.Equal(s.T(), s.NumPorts + 3, len(processes)) // sleep + sh -c + seq

	processMap := getSocatProcessesByPort(processes)
	endpointMap := getEndpointsByPort(endpoints)

	for port := 1; port <= s.NumPorts; port++ {
		assert.Equal(s.T(), 1, len(endpointMap[port]))
		assert.Equal(s.T(), 1, len(processMap[port]))
		assert.Equal(s.T(), "L4_PROTOCOL_TCP", endpointMap[port][0].Protocol)
		assert.Equal(s.T(), endpointMap[port][0].Originator.ProcessName, processMap[port][0].Name)
		assert.Equal(s.T(), endpointMap[port][0].Originator.ProcessExecFilePath, processMap[port][0].ExePath)
		assert.Equal(s.T(), endpointMap[port][0].Originator.ProcessArgs, processMap[port][0].Args)
		assert.Equal(s.T(), port, endpointMap[port][0].Address.Port)
	}
}

func getSocatProcessPort(process common.ProcessInfo) int {
	re := regexp.MustCompile(`:([0-9]+),`)
	portArr := re.FindStringSubmatch(process.Args)
	if len(portArr) == 2 {
		port, _ := strconv.Atoi(portArr[1])
		return port
	}

	return -1
}

func getSocatProcessesByPort(processes []common.ProcessInfo) (map[int][]common.ProcessInfo) {
	processMap := make(map[int][]common.ProcessInfo)
	for _, process := range processes {
		if process.Name == "socat" {
			port := getSocatProcessPort(process)
			processMap[port] = append(processMap[port], process)
		}
	}
	return processMap
}

func getEndpointsByPort(endpoints []common.EndpointInfo) (map[int][]common.EndpointInfo) {
	endpointMap := make(map[int][]common.EndpointInfo)
	for _, endpoint := range endpoints {
		port := endpoint.Address.Port
		endpointMap[port] = append(endpointMap[port], endpoint)
	}

	return endpointMap
}
