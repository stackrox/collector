package integrationtests

import (
	"fmt"
	"os"
	"regexp"
	"strconv"
	"strings"
	"testing"
	"time"

	"encoding/json"

	"github.com/boltdb/bolt"
	"github.com/gonum/stat"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/stretchr/testify/suite"
)

const (
	processBucket            = "Process"
	processLineageInfoBucket = "LineageInfo"
	networkBucket            = "Network"
	parentUIDStr             = "ParentUid"
	parentExecFilePathStr    = "ParentExecFilePath"
)

func TestBenchmarkBaseline(t *testing.T) {
	suite.Run(t, new(BenchmarkBaselineTestSuite))
}

func TestCollectorGRPC(t *testing.T) {
	suite.Run(t, new(ProcessNetworkTestSuite))
	suite.Run(t, new(BenchmarkCollectorTestSuite))
}

func TestProcessNetwork(t *testing.T) {
	suite.Run(t, new(ProcessNetworkTestSuite))
}

func TestBenchmark(t *testing.T) {
	suite.Run(t, new(BenchmarkCollectorTestSuite))
}

func TestImageLabelJSON(t *testing.T) {
	suite.Run(t, new(ImageLabelJSONTestSuite))
}

// TestMissingProcScrape only works with local fake proc directory
func TestMissingProcScrape(t *testing.T) {
	if ReadEnvVarWithDefault("REMOTE_HOST_TYPE", "local") == "local" {
		suite.Run(t, new(MissingProcScrapeTestSuite))
	}
}

func TestRepeatedNetworkFlow(t *testing.T) {
	//Perform 10 curl commands with a 1 second sleep between each curl command
	//The first client to server connection is recorded
	//The first server to client connection is recorded
	//The second through ninth curl commands are ignored, because of afterglow
	//The closing of the last server connection is recorded
	//The closing of the last client connection is recorded
	//Thus there are 4 networking events recorded
	repeatedNetworkFlowTestSuite := new(RepeatedNetworkFlowTestSuite)
	/*
	repeatedNetworkFlowTestSuite.numMetaIter = 1
	repeatedNetworkFlowTestSuite.numIter = 10
	repeatedNetworkFlowTestSuite.sleepBetweenCurlTime = 1
	repeatedNetworkFlowTestSuite.sleepBetweenIterations = 1
	repeatedNetworkFlowTestSuite.expectedReports = 2
	suite.Run(t, repeatedNetworkFlowTestSuite)

	//Perform two curl commands 40 seconds apart
	//40 seconds is greater than the afterglow period so all openings and closings are reported
	//Every opening and closing for the server and client are reported and there are two curls
	//so we have 2*2*2=8 recoreded networking events
	repeatedNetworkFlowTestSuite.numMetaIter = 1
	repeatedNetworkFlowTestSuite.numIter = 2
	repeatedNetworkFlowTestSuite.sleepBetweenCurlTime = 40
	repeatedNetworkFlowTestSuite.sleepBetweenIterations = 1
	repeatedNetworkFlowTestSuite.expectedReports = 4
	suite.Run(t, repeatedNetworkFlowTestSuite)
	*/

	//Perform a curl
	//wait one second
	//perform a curl
	//wait 40 seconds
	//perform a curl
	//wait one second
	//perform a curl
	//We should get 4 recorded networking events, for the same reason that we 4 records from the
	//first test. Since there is a 40 second sleep between the two sets of curl commands and that is
	//longer than the afterglow period, the two sets of curl commands are independent. Thus we get
	//2*4=8 records
	repeatedNetworkFlowTestSuite.numMetaIter = 2
	repeatedNetworkFlowTestSuite.numIter = 2
	repeatedNetworkFlowTestSuite.sleepBetweenCurlTime = 1
	repeatedNetworkFlowTestSuite.sleepBetweenIterations = 40
	repeatedNetworkFlowTestSuite.expectedReports = 4
	suite.Run(t, repeatedNetworkFlowTestSuite)

	repeatedNetworkFlowTestSuite.numMetaIter = 1
	repeatedNetworkFlowTestSuite.numIter = 4
	repeatedNetworkFlowTestSuite.sleepBetweenCurlTime = 1
	repeatedNetworkFlowTestSuite.sleepBetweenIterations = 1
	repeatedNetworkFlowTestSuite.expectedReports = 2
	suite.Run(t, repeatedNetworkFlowTestSuite)

	repeatedNetworkFlowTestSuite.numMetaIter = 1
	repeatedNetworkFlowTestSuite.numIter = 4
	repeatedNetworkFlowTestSuite.sleepBetweenCurlTime = 15
	repeatedNetworkFlowTestSuite.sleepBetweenIterations = 1
	repeatedNetworkFlowTestSuite.expectedReports = 2
	suite.Run(t, repeatedNetworkFlowTestSuite)

	repeatedNetworkFlowTestSuite.numMetaIter = 1
	repeatedNetworkFlowTestSuite.numIter = 4
	repeatedNetworkFlowTestSuite.sleepBetweenCurlTime = 18
	repeatedNetworkFlowTestSuite.sleepBetweenIterations = 1
	repeatedNetworkFlowTestSuite.expectedReports = 2
	suite.Run(t, repeatedNetworkFlowTestSuite)

	repeatedNetworkFlowTestSuite.numMetaIter = 1
	repeatedNetworkFlowTestSuite.numIter = 4
	repeatedNetworkFlowTestSuite.sleepBetweenCurlTime = 22
	repeatedNetworkFlowTestSuite.sleepBetweenIterations = 1
	repeatedNetworkFlowTestSuite.expectedReports = 8
	suite.Run(t, repeatedNetworkFlowTestSuite)

	repeatedNetworkFlowTestSuite.numMetaIter = 1
	repeatedNetworkFlowTestSuite.numIter = 4
	repeatedNetworkFlowTestSuite.sleepBetweenCurlTime = 25
	repeatedNetworkFlowTestSuite.sleepBetweenIterations = 1
	repeatedNetworkFlowTestSuite.expectedReports = 8
	suite.Run(t, repeatedNetworkFlowTestSuite)

	repeatedNetworkFlowTestSuite.numMetaIter = 1
	repeatedNetworkFlowTestSuite.numIter = 4
	repeatedNetworkFlowTestSuite.sleepBetweenCurlTime = 30
	repeatedNetworkFlowTestSuite.sleepBetweenIterations = 1
	repeatedNetworkFlowTestSuite.expectedReports = 8
	suite.Run(t, repeatedNetworkFlowTestSuite)
}

type IntegrationTestSuiteBase struct {
	suite.Suite
	db        *bolt.DB
	executor  Executor
	collector *collectorManager
	metrics   map[string]float64
}

type ProcessNetworkTestSuite struct {
	IntegrationTestSuiteBase
	clientContainer string
	clientIP        string
	serverContainer string
	serverIP        string
	serverPort      string
}

type ContainerStat struct {
	Timestamp string
	Id        string
	Name      string
	Mem       string
	Cpu       float64
}

type PerformanceResult struct {
	TestName         string
	VmConfig         string
	CollectionMethod string
	Metrics          map[string]float64
	ContainerStats   []ContainerStat
}

type BenchmarkCollectorTestSuite struct {
	IntegrationTestSuiteBase
}

type BenchmarkBaselineTestSuite struct {
	IntegrationTestSuiteBase
}

type MissingProcScrapeTestSuite struct {
	IntegrationTestSuiteBase
}

type RepeatedNetworkFlowTestSuite struct {
	IntegrationTestSuiteBase
	clientContainer string
	clientIP        string
	serverContainer string
	serverIP        string
	serverPort      string
	numMetaIter	int
	numIter		int
	sleepBetweenCurlTime	int
	sleepBetweenIterations	int
	expectedReports		int
	observedReports		int
}

type ImageLabelJSONTestSuite struct {
	IntegrationTestSuiteBase
}

func (s *ImageLabelJSONTestSuite) SetupSuite() {
	s.executor = NewExecutor()
	s.collector = NewCollectorManager(s.executor, s.T().Name())
	err := s.collector.Setup()
	require.NoError(s.T(), err)
	err = s.collector.Launch()
	require.NoError(s.T(), err)
}

func (s *ImageLabelJSONTestSuite) TestRunImageWithJSONLabel() {
	s.RunImageWithJSONLabels()
}

func (s *ImageLabelJSONTestSuite) TearDownSuite() {
	err := s.collector.TearDown()
	require.NoError(s.T(), err)
	s.db, err = s.collector.BoltDB()
	require.NoError(s.T(), err)
	s.cleanupContainer([]string{"collector", "grpc-server", "jsonlabel"})
}

func (s *BenchmarkCollectorTestSuite) SetupSuite() {
	s.executor = NewExecutor()
	s.StartContainerStats()
	s.collector = NewCollectorManager(s.executor, s.T().Name())
	s.metrics = map[string]float64{}

	err := s.collector.Setup()
	require.NoError(s.T(), err)

	err = s.collector.Launch()
	require.NoError(s.T(), err)

}

func (s *BenchmarkCollectorTestSuite) TestBenchmarkCollector() {
	s.RunCollectorBenchmark()
}

func (s *BenchmarkCollectorTestSuite) TearDownSuite() {
	err := s.collector.TearDown()
	require.NoError(s.T(), err)

	s.db, err = s.collector.BoltDB()
	require.NoError(s.T(), err)

	s.cleanupContainer([]string{"collector", "grpc-server", "benchmark"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("collector_benchmark", stats, s.metrics)
}

func (s *BenchmarkBaselineTestSuite) SetupSuite() {
	s.executor = NewExecutor()
	s.metrics = map[string]float64{}
	s.StartContainerStats()
}

func (s *BenchmarkBaselineTestSuite) TestBenchmarkBaseline() {
	s.RunCollectorBenchmark()
}

func (s *BenchmarkBaselineTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"benchmark"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("baseline_benchmark", stats, s.metrics)
}

// Launches collector
// Launches gRPC server in insecure mode
// Launches nginx container
// Execs into nginx and does a sleep
func (s *ProcessNetworkTestSuite) SetupSuite() {

	s.metrics = map[string]float64{}
	s.executor = NewExecutor()
	s.StartContainerStats()
	s.collector = NewCollectorManager(s.executor, s.T().Name())

	err := s.collector.Setup()
	require.NoError(s.T(), err)

	err = s.collector.Launch()
	require.NoError(s.T(), err)

	images := []string{
		"nginx:1.14-alpine",
		"pstauffer/curl:latest",
	}

	for _, image := range images {
		err := s.executor.PullImage(image)
		require.NoError(s.T(), err)
	}

	time.Sleep(10 * time.Second)

	// invokes default nginx
	containerID, err := s.launchContainer("nginx", "nginx:1.14-alpine")
	require.NoError(s.T(), err)
	s.serverContainer = containerID[0:12]

	// invokes "sleep" and "sh" and "ls"
	_, err = s.execContainer("nginx", []string{"sleep", "5"})
	require.NoError(s.T(), err)
	_, err = s.execContainer("nginx", []string{"sh", "-c", "ls"})
	require.NoError(s.T(), err)

	// invokes another container
	containerID, err = s.launchContainer("nginx-curl", "pstauffer/curl:latest", "sleep", "300")
	require.NoError(s.T(), err)
	s.clientContainer = containerID[0:12]

	s.serverIP, err = s.getIPAddress("nginx")
	require.NoError(s.T(), err)

	s.serverPort, err = s.getPort("nginx")
	require.NoError(s.T(), err)

	_, err = s.execContainer("nginx-curl", []string{"curl", fmt.Sprintf("%s:%s", s.serverIP, s.serverPort)})
	require.NoError(s.T(), err)

	s.clientIP, err = s.getIPAddress("nginx-curl")
	require.NoError(s.T(), err)

	time.Sleep(10 * time.Second)

	err = s.collector.TearDown()
	require.NoError(s.T(), err)

	s.db, err = s.collector.BoltDB()
	require.NoError(s.T(), err)
}

func (s *ProcessNetworkTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"nginx", "nginx-curl", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("process_network", stats, s.metrics)
}

func (s *ProcessNetworkTestSuite) TestProcessViz() {
	processName := "nginx"
	exeFilePath := "/usr/sbin/nginx"
	expectedProcessInfo := fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err := s.Get(processName, processBucket)
	require.NoError(s.T(), err)
	assert.Equal(s.T(), expectedProcessInfo, val)

	processName = "sh"
	exeFilePath = "/bin/sh"
	expectedProcessInfo = fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err = s.Get(processName, processBucket)
	require.NoError(s.T(), err)
	assert.Equal(s.T(), expectedProcessInfo, val)

	processName = "sleep"
	exeFilePath = "/bin/sleep"
	expectedProcessInfo = fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err = s.Get(processName, processBucket)
	require.NoError(s.T(), err)
	assert.Equal(s.T(), expectedProcessInfo, val)
}

func (s *ProcessNetworkTestSuite) TestProcessLineageInfo() {
	processName := "awk"
	exeFilePath := "/usr/bin/awk"
	parentFilePath := "/bin/busybox"
	expectedProcessLineageInfo := fmt.Sprintf("%s:%s:%s:%d:%s:%s", processName, exeFilePath, parentUIDStr, 0, parentExecFilePathStr, parentFilePath)
	val, err := s.GetLineageInfo(processName, "0", processLineageInfoBucket)
	require.NoError(s.T(), err)
	assert.Equal(s.T(), expectedProcessLineageInfo, val)

	processName = "grep"
	exeFilePath = "/bin/grep"
	parentFilePath = "/bin/busybox"
	expectedProcessLineageInfo = fmt.Sprintf("%s:%s:%s:%d:%s:%s", processName, exeFilePath, parentUIDStr, 0, parentExecFilePathStr, parentFilePath)
	val, err = s.GetLineageInfo(processName, "0", processLineageInfoBucket)
	require.NoError(s.T(), err)
	assert.Equal(s.T(), expectedProcessLineageInfo, val)

	processName = "sleep"
	exeFilePath = "/bin/sleep"
	parentFilePath = "/bin/busybox"
	expectedProcessLineageInfo = fmt.Sprintf("%s:%s:%s:%d:%s:%s", processName, exeFilePath, parentUIDStr, 0, parentExecFilePathStr, parentFilePath)
	val, err = s.GetLineageInfo(processName, "0", processLineageInfoBucket)
	require.NoError(s.T(), err)
	assert.Equal(s.T(), expectedProcessLineageInfo, val)
}

func (s *ProcessNetworkTestSuite) TestNetworkFlows() {

	// Server side checks
	val, err := s.Get(s.serverContainer, networkBucket)
	require.NoError(s.T(), err)
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
	require.NoError(s.T(), err)
	actualValues = strings.Split(string(val), "|")

	actualClientEndpoint = actualValues[0]
	actualServerEndpoint = actualValues[1]

	// From client perspective, network connection info has no local endpoint and full remote endpoint
	assert.Empty(s.T(), actualClientEndpoint)
	assert.Equal(s.T(), fmt.Sprintf("%s:%s", s.serverIP, s.serverPort), actualServerEndpoint)

	fmt.Printf("ClientDetails from Bolt: %s %s\n", s.clientContainer, string(val))
	fmt.Printf("ClientDetails from test: %s %s\n", s.clientContainer, s.clientIP)
}

func (s *MissingProcScrapeTestSuite) SetupSuite() {
	_, err := os.Stat("/tmp/fake-proc")
	assert.False(s.T(), os.IsNotExist(err), "Missing fake proc directory")

	s.executor = NewExecutor()
	s.collector = NewCollectorManager(s.executor, s.T().Name())

	// Mount the fake proc directory created by 'create-fake-proc.sh'
	s.collector.Mounts["/host/proc:ro"] = "/tmp/fake-proc"

	err = s.collector.Setup()
	require.NoError(s.T(), err)

	err = s.collector.Launch()
	require.NoError(s.T(), err)

	time.Sleep(10 * time.Second)
}

func (s *MissingProcScrapeTestSuite) TestCollectorRunning() {
	collectorRunning, err := s.executor.IsContainerRunning("collector")
	require.NoError(s.T(), err)
	assert.True(s.T(), collectorRunning, "Collector isn't running")
}

func (s *MissingProcScrapeTestSuite) TearDownSuite() {
	err := s.collector.TearDown()
	require.NoError(s.T(), err)
	s.cleanupContainer([]string{"collector"})
}

// Launches collector
// Launches gRPC server in insecure mode
// Launches nginx container
func (s *RepeatedNetworkFlowTestSuite) SetupSuite() {
	s.metrics = map[string]float64{}
	s.executor = NewExecutor()
	s.StartContainerStats()
	s.collector = NewCollectorManager(s.executor, s.T().Name())

	err := s.collector.Setup()
	require.NoError(s.T(), err)

	err = s.collector.Launch()
	require.NoError(s.T(), err)

	images := []string{
		"nginx:1.14-alpine",
		"pstauffer/curl:latest",
	}

	for _, image := range images {
		err := s.executor.PullImage(image)
		require.NoError(s.T(), err)
	}

	time.Sleep(10 * time.Second)

	// invokes default nginx
	containerID, err := s.launchContainer("nginx", "nginx:1.14-alpine")
	require.NoError(s.T(), err)
	s.serverContainer = containerID[0:12]

	// invokes another container
	containerID, err = s.launchContainer("nginx-curl", "pstauffer/curl:latest", "sleep", "300")
	require.NoError(s.T(), err)
	s.clientContainer = containerID[0:12]

	s.serverIP, err = s.getIPAddress("nginx")
	require.NoError(s.T(), err)

	s.serverPort, err = s.getPort("nginx")
	require.NoError(s.T(), err)
	time.Sleep(20 * time.Second)

	logLinesInitial := s.GetLogLines("grpc-server")
	networkLinesInitial := CountNumMatchingPattern(logLinesInitial, "^Network")
	serverAddress := fmt.Sprintf("%s:%s", s.serverIP, s.serverPort)
	/*
	innerLoop := "for j in `seq 0 " + strconv.Itoa(s.numIter - 1)  + "`; do curl " + serverAddress + `; sleep ` + strconv.Itoa(s.sleepBetweenCurlTime) + `; done`
	curlCommand := "for i in `seq 0 " + strconv.Itoa(s.numMetaIter - 1) + "`; do " + innerLoop + `; sleep ` + strconv.Itoa(s.sleepBetweenIterations) + `; done`
	_, err = s.execContainer("nginx-curl", []string{"sh", "-c", curlCommand})
	*/
	//innerLoop := `j=0; while [ $j -lt ` + strconv.Itoa(s.numIter) + ` ]; do curl ` + serverAddress + `; sleep ` + strconv.Itoa(s.sleepBetweenCurlTime) + `; j=$((j + 1)); done`
	//curlCommand := `i=0; while [ $i -lt ` + strconv.Itoa(s.numMetaIter) + ` ]; do ` + innerLoop + `; sleep ` + strconv.Itoa(s.sleepBetweenIterations) + `; i=$((i + 1)); done`

	for i := 0; i < s.numMetaIter; i++ {
		for j := 0; j < s.numIter; j++ {
			_, err = s.execContainer("nginx-curl", []string{"curl", serverAddress})
			require.NoError(s.T(), err)
			time.Sleep(time.Duration(s.sleepBetweenCurlTime) * time.Second)
		}
		time.Sleep(time.Duration(s.sleepBetweenIterations) * time.Second)
	}

	s.clientIP, err = s.getIPAddress("nginx-curl")
	require.NoError(s.T(), err)

	totalTime := s.sleepBetweenCurlTime * s.numIter * s.numMetaIter + s.sleepBetweenIterations * s.numMetaIter
	totalTime = totalTime + 20
	time.Sleep(time.Duration(totalTime) * time.Second)
	logLines := s.GetLogLines("grpc-server")
	networkLines := CountNumMatchingPattern(logLines, "^Network")
	s.observedReports = networkLines - networkLinesInitial
	//This should be in TestRepeatedNetworkFlow, but I was unable to access the grpc-serve log files
	//there, or get the number of recorded networking events in any other way
	//assert.Equal(s.T(), s.expectedReports, networkLines)
	err = s.collector.TearDown()
	require.NoError(s.T(), err)

	s.db, err = s.collector.BoltDB()
	require.NoError(s.T(), err)
}

func (s *RepeatedNetworkFlowTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"nginx", "nginx-curl", "collector"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("repeated_network_flow", stats, s.metrics)
}

func (s *RepeatedNetworkFlowTestSuite) TestRepeatedNetworkFlow() {
	// Server side checks
	assert.Equal(s.T(), s.expectedReports, s.observedReports)

	val, err := s.Get(s.serverContainer, networkBucket)
	require.NoError(s.T(), err)
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
	require.NoError(s.T(), err)
	actualValues = strings.Split(string(val), "|")

	actualClientEndpoint = actualValues[0]
	actualServerEndpoint = actualValues[1]

	// From client perspective, network connection info has no local endpoint and full remote endpoint
	assert.Empty(s.T(), actualClientEndpoint)
	assert.Equal(s.T(), fmt.Sprintf("%s:%s", s.serverIP, s.serverPort), actualServerEndpoint)

	fmt.Printf("ClientDetails from Bolt: %s %s\n", s.clientContainer, string(val))
	fmt.Printf("ClientDetails from test: %s %s\n", s.clientContainer, s.clientIP)
}

func (s *IntegrationTestSuiteBase) launchContainer(args ...string) (string, error) {
	cmd := []string{"docker", "run", "-d", "--name"}
	cmd = append(cmd, args...)
	output, err := s.executor.Exec(cmd...)
	outLines := strings.Split(output, "\n")
	return outLines[len(outLines)-1], err
}

func (s *IntegrationTestSuiteBase) waitForContainerToExit(containerName, containerID string) (bool, error) {
	cmd := []string{
		"docker", "ps", "-qa",
		"--filter", "id=" + containerID,
		"--filter", "status=exited",
	}

	start := time.Now()
	tick := time.Tick(30 * time.Second)
	tickElapsed := time.Tick(1 * time.Minute)
	timeout := time.After(15 * time.Minute)
	for {
		select {
		case <-tick:
			output, err := s.executor.Exec(cmd...)
			outLines := strings.Split(output, "\n")
			if outLines[len(outLines)-1] == containerID {
				return true, nil
			}
			if err != nil {
				fmt.Printf("Retrying waitForContainerToExit(%s, %s): Error: %v\n", containerName, containerID, err)
			}
		case <-timeout:
			fmt.Printf("Timed out waiting for container %s to exit, elapsed Time: %s\n", containerName, time.Since(start))
			return false, nil
		case <-tickElapsed:
			fmt.Printf("Waiting for container: %s, elapsed time: %s\n", containerName, time.Since(start))
		}
	}
}

func (s *IntegrationTestSuiteBase) execContainer(containerName string, command []string) (string, error) {
	cmd := []string{"docker", "exec", containerName}
	cmd = append(cmd, command...)
	return s.executor.Exec(cmd...)
}

func (s *IntegrationTestSuiteBase) cleanupContainer(containers []string) {
	for _, container := range containers {
		s.executor.Exec("docker", "kill", container)
		s.executor.Exec("docker", "rm", container)
	}
}

func (s *IntegrationTestSuiteBase) containerLogs(containerName string) (string, error) {
	return s.executor.Exec("docker", "logs", containerName)
}

func (s *IntegrationTestSuiteBase) getIPAddress(containerName string) (string, error) {
	stdoutStderr, err := s.executor.Exec("docker", "inspect", "--format='{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'", containerName)
	return strings.Replace(string(stdoutStderr), "'", "", -1), err
}

func (s *IntegrationTestSuiteBase) getPort(containerName string) (string, error) {
	stdoutStderr, err := s.executor.Exec("docker", "inspect", "--format='{{json .NetworkSettings.Ports}}'", containerName)
	if err != nil {
		return "", err
	}
	rawString := strings.Trim(string(stdoutStderr), "'\n")
	var portMap map[string]interface{}
	err = json.Unmarshal([]byte(rawString), &portMap)
	if err != nil {
		return "", err
	}

	for k := range portMap {
		return strings.Split(k, "/")[0], nil
	}

	return "", fmt.Errorf("no port mapping found: %v %v", rawString, portMap)
}

func (s *IntegrationTestSuiteBase) Get(key string, bucket string) (val string, err error) {
	if s.db == nil {
		return "", fmt.Errorf("Db %v is nil", s.db)
	}
	err = s.db.View(func(tx *bolt.Tx) error {
		b := tx.Bucket([]byte(bucket))
		if b == nil {
			return fmt.Errorf("Bucket %s was not found", bucket)
		}
		val = string(b.Get([]byte(key)))
		return nil
	})
	return
}

func (s *IntegrationTestSuiteBase) GetLineageInfo(processName string, key string, bucket string) (val string, err error) {
	if s.db == nil {
		return "", fmt.Errorf("Db %v is nil", s.db)
	}
	err = s.db.View(func(tx *bolt.Tx) error {
		b := tx.Bucket([]byte(bucket))

		if b == nil {
			return fmt.Errorf("Bucket %s was not found", bucket)
		}

		processBucket := b.Bucket([]byte(processName))
		if processBucket == nil {
			return fmt.Errorf("Process bucket %s was not found", processName)
		}
		val = string(processBucket.Get([]byte(key)))
		return nil
	})
	return
}

func (s *IntegrationTestSuiteBase) RunCollectorBenchmark() {
	benchmarkName := "benchmark"
	benchmarkImage := "stackrox/benchmark-collector:phoronix"

	err := s.executor.PullImage(benchmarkImage)
	require.NoError(s.T(), err)

	benchmarkArgs := []string{
		benchmarkName,
		"--env", "FORCE_TIMES_TO_RUN=1",
		benchmarkImage,
		"phoronix-test-suite", "batch-benchmark", "collector",
	}

	containerID, err := s.launchContainer(benchmarkArgs...)
	require.NoError(s.T(), err)
	benchmarkContainerID := containerID[0:12]

	_, err = s.waitForContainerToExit(benchmarkName, benchmarkContainerID)
	require.NoError(s.T(), err)

	benchmarkLogs, err := s.containerLogs("benchmark")
	re := regexp.MustCompile(`Average: ([0-9.]+) Seconds`)
	matches := re.FindSubmatch([]byte(benchmarkLogs))
	if matches != nil {
		fmt.Printf("Benchmark Time: %s\n", matches[1])
		f, err := strconv.ParseFloat(string(matches[1]), 64)
		require.NoError(s.T(), err)
		s.metrics["hackbench_avg_time"] = f
	} else {
		fmt.Printf("Benchmark Time: Not found! Logs: %s\n", benchmarkLogs)
		assert.FailNow(s.T(), "Benchmark Time not found")
	}
}

func (s *IntegrationTestSuiteBase) RunImageWithJSONLabels() {
	name := "jsonlabel"
	image := "stackrox/benchmark-collector:json-label"
	err := s.executor.PullImage(image)
	require.NoError(s.T(), err)
	args := []string{
		name,
		image,
	}
	containerID, err := s.launchContainer(args...)
	require.NoError(s.T(), err)
	_, err = s.waitForContainerToExit(name, containerID[0:12])
	require.NoError(s.T(), err)
}

func (s *IntegrationTestSuiteBase) StartContainerStats() {
	name := "container-stats"
	image := "stackrox/benchmark-collector:stats"
	args := []string{name, "-v", "/var/run/docker.sock:/var/run/docker.sock", image}

	err := s.executor.PullImage(image)
	require.NoError(s.T(), err)

	_, err = s.launchContainer(args...)
	require.NoError(s.T(), err)
}

func (s *IntegrationTestSuiteBase) GetLogLines(containerName string) ([]string) {
	logs, err := s.containerLogs(containerName)
	if err != nil {
		assert.FailNow(s.T(), containerName + " failure")
	}
	logLines := strings.Split(logs, "\n")
	return logLines
}

func CountNumMatchingPattern(lines []string, pattern string) int {
	count := 0
	for i := range lines {
		foundMatch, _ := regexp.MatchString(pattern, lines[i])
		if foundMatch {
			count++
		}
	}
	return count
}

func (s *IntegrationTestSuiteBase) GetContainerStats() (stats []ContainerStat) {
	logs, err := s.containerLogs("container-stats")
	if err != nil {
		assert.FailNow(s.T(), "container-stats failure")
		return nil
	}
	logLines := strings.Split(logs, "\n")
	for _, line := range logLines {
		var stat ContainerStat
		json.Unmarshal([]byte(line), &stat)
		stats = append(stats, stat)
	}
	s.cleanupContainer([]string{"container-stats"})
	return stats
}

func (s *IntegrationTestSuiteBase) PrintContainerStats(stats []ContainerStat) {
	cpuStats := map[string][]float64{}
	for _, stat := range stats {
		cpuStats[stat.Name] = append(cpuStats[stat.Name], stat.Cpu)
	}
	for name, cpu := range cpuStats {
		s.metrics[fmt.Sprintf("%s_cpu_mean", name)] = stat.Mean(cpu, nil)
		s.metrics[fmt.Sprintf("%s_cpu_stddev", name)] = stat.StdDev(cpu, nil)

		fmt.Printf("CPU: Container %s, Mean %v, StdDev %v\n",
			name, stat.Mean(cpu, nil), stat.StdDev(cpu, nil))
	}
}

func (s *IntegrationTestSuiteBase) WritePerfResults(testName string, stats []ContainerStat, metrics map[string]float64) {
	perf := PerformanceResult{
		TestName:         testName,
		VmConfig:         ReadEnvVarWithDefault("VM_CONFIG", "default"),
		CollectionMethod: ReadEnvVarWithDefault("COLLECTION_METHOD", "kernel_module"),
		Metrics:          metrics,
		ContainerStats:   stats,
	}

	perfJson, _ := json.Marshal(perf)
	perfFilename := "perf.json"

	fmt.Printf("Writing %s\n", perfFilename)
	f, err := os.OpenFile(perfFilename, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	require.NoError(s.T(), err)
	defer f.Close()

	_, err = f.WriteString(string(perfJson))
	require.NoError(s.T(), err)
}
