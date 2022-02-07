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

//Need to update the comments in these tests
func TestRepeatedNetworkFlow(t *testing.T) {
	// Perform 11 curl commands with a 2 second sleep between each curl command.
	// The scrapeInterval is increased to 4 seconds to reduce the chance that jiter will effect the results.
	// The first client to server connection is recorded as being active.
	// The first server to client connection is recorded as being active.
	// The second through ninth curl commands are ignored, because of afterglow.
	// The last client to server connection is recorded as being inacitve when the afterglow period has expired
	// The last server to client connection is recorded as being inacitve when the afterglow period has expired
	// Thus the reported connections are active, active, inactive inactive
	repeatedNetworkFlowTestSuite := &RepeatedNetworkFlowTestSuite{
		afterglowPeriod: 10,
		scrapeInterval: 4,
		enableAfterglow: true,
		numMetaIter: 1,
		numIter: 11,
		sleepBetweenCurlTime: 2,
		sleepBetweenIterations: 1,
		expectedReports: []bool{true, true, false, false},
	}
	suite.Run(t, repeatedNetworkFlowTestSuite)
}

func TestRepeatedNetworkFlowWithZeroAfterglowPeriod(t *testing.T) {
	// Similar to TestRepeatedNetworkFlowLongPauseNoAfterglow we see the patern false, false 3 times
	// instead of twice.
	repeatedNetworkFlowTestSuite := &RepeatedNetworkFlowTestSuite{
		afterglowPeriod: 0,
		scrapeInterval: 2,
		enableAfterglow: true,
		numMetaIter: 1,
		numIter: 3,
		sleepBetweenCurlTime: 3,
		sleepBetweenIterations: 1,
		expectedReports: []bool{false, false, false, false, false, false},
	}
	suite.Run(t, repeatedNetworkFlowTestSuite)
}

func TestRepeatedNetworkFlowThreeCurlsNoAfterglow(t *testing.T) {
	// The afterglow period is set to 0 so this has the same behavior as if afterglow was disabled.
	repeatedNetworkFlowTestSuite := &RepeatedNetworkFlowTestSuite{
		afterglowPeriod: 0,
		scrapeInterval: 2,
		enableAfterglow: false,
		numMetaIter: 1,
		numIter: 3,
		sleepBetweenCurlTime: 3,
		sleepBetweenIterations: 1,
		expectedReports: []bool{false, false, false, false, false, false},
	}
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
	//The goal with these integration tests is to make sure we report the correct number of
	//networking events. Sometimes if a connection is made multiple times within a short time
	//called an "afterglow" period, we only want to report the connection once.
	IntegrationTestSuiteBase
	clientContainer string
	clientIP        string
	serverContainer string
	serverIP        string
	serverPort      string
	enableAfterglow	bool
	afterglowPeriod	int
	scrapeInterval	int
	numMetaIter	int
	numIter		int
	sleepBetweenCurlTime	int
	sleepBetweenIterations	int
	expectedReports		[]bool // An array of booleans representing the connection. true is active. fasle is inactive.
	observedReports		[]bool
}

type ImageLabelJSONTestSuite struct {
	IntegrationTestSuiteBase
}

func (s *ImageLabelJSONTestSuite) SetupSuite() {
	s.executor = NewExecutor()
	s.collector = NewCollectorManager(s.executor, s.T().Name())
	err := s.collector.Setup()
	s.Require().NoError(err)
	err = s.collector.Launch()
	s.Require().NoError(err)
}

func (s *ImageLabelJSONTestSuite) TestRunImageWithJSONLabel() {
	s.RunImageWithJSONLabels()
}

func (s *ImageLabelJSONTestSuite) TearDownSuite() {
	err := s.collector.TearDown()
	s.Require().NoError(err)
	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
	s.cleanupContainer([]string{"collector", "grpc-server", "jsonlabel"})
}

func (s *BenchmarkCollectorTestSuite) SetupSuite() {
	s.executor = NewExecutor()
	s.StartContainerStats()
	s.collector = NewCollectorManager(s.executor, s.T().Name())
	s.metrics = map[string]float64{}

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)

}

func (s *BenchmarkCollectorTestSuite) TestBenchmarkCollector() {
	s.RunCollectorBenchmark()
}

func (s *BenchmarkCollectorTestSuite) TearDownSuite() {
	err := s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)

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
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)

	images := []string{
		"nginx:1.14-alpine",
		"pstauffer/curl:latest",
	}

	for _, image := range images {
		err := s.executor.PullImage(image)
		s.Require().NoError(err)
	}

	time.Sleep(10 * time.Second)

	// invokes default nginx
	containerID, err := s.launchContainer("nginx", "nginx:1.14-alpine")
	s.Require().NoError(err)
	s.serverContainer = containerID[0:12]

	// invokes "sleep" and "sh" and "ls"
	_, err = s.execContainer("nginx", []string{"sleep", "5"})
	s.Require().NoError(err)
	_, err = s.execContainer("nginx", []string{"sh", "-c", "ls"})
	s.Require().NoError(err)

	// invokes another container
	containerID, err = s.launchContainer("nginx-curl", "pstauffer/curl:latest", "sleep", "300")
	s.Require().NoError(err)
	s.clientContainer = containerID[0:12]

	s.serverIP, err = s.getIPAddress("nginx")
	s.Require().NoError(err)

	s.serverPort, err = s.getPort("nginx")
	s.Require().NoError(err)

	_, err = s.execContainer("nginx-curl", []string{"curl", fmt.Sprintf("%s:%s", s.serverIP, s.serverPort)})
	s.Require().NoError(err)

	s.clientIP, err = s.getIPAddress("nginx-curl")
	s.Require().NoError(err)

	time.Sleep(10 * time.Second)

	err = s.collector.TearDown()
	s.Require().NoError(err)

	s.db, err = s.collector.BoltDB()
	s.Require().NoError(err)
}

func (s *ProcessNetworkTestSuite) TearDownSuite() {
	s.cleanupContainer([]string{"nginx", "nginx-curl"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("process_network", stats, s.metrics)
}

func (s *ProcessNetworkTestSuite) TestProcessViz() {
	processName := "nginx"
	exeFilePath := "/usr/sbin/nginx"
	expectedProcessInfo := fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err := s.Get(processName, processBucket)
	s.Require().NoError(err)
	assert.Equal(s.T(), expectedProcessInfo, val)

	processName = "sh"
	exeFilePath = "/bin/sh"
	expectedProcessInfo = fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err = s.Get(processName, processBucket)
	s.Require().NoError(err)
	assert.Equal(s.T(), expectedProcessInfo, val)

	processName = "sleep"
	exeFilePath = "/bin/sleep"
	expectedProcessInfo = fmt.Sprintf("%s:%s:%d:%d", processName, exeFilePath, 0, 0)
	val, err = s.Get(processName, processBucket)
	s.Require().NoError(err)
	assert.Equal(s.T(), expectedProcessInfo, val)
}

func (s *ProcessNetworkTestSuite) TestProcessLineageInfo() {
	processName := "awk"
	exeFilePath := "/usr/bin/awk"
	parentFilePath := "/bin/busybox"
	expectedProcessLineageInfo := fmt.Sprintf("%s:%s:%s:%d:%s:%s", processName, exeFilePath, parentUIDStr, 0, parentExecFilePathStr, parentFilePath)
	val, err := s.GetLineageInfo(processName, "0", processLineageInfoBucket)
	s.Require().NoError(err)
	assert.Equal(s.T(), expectedProcessLineageInfo, val)

	processName = "grep"
	exeFilePath = "/bin/grep"
	parentFilePath = "/bin/busybox"
	expectedProcessLineageInfo = fmt.Sprintf("%s:%s:%s:%d:%s:%s", processName, exeFilePath, parentUIDStr, 0, parentExecFilePathStr, parentFilePath)
	val, err = s.GetLineageInfo(processName, "0", processLineageInfoBucket)
	s.Require().NoError(err)
	assert.Equal(s.T(), expectedProcessLineageInfo, val)

	processName = "sleep"
	exeFilePath = "/bin/sleep"
	parentFilePath = "/bin/busybox"
	expectedProcessLineageInfo = fmt.Sprintf("%s:%s:%s:%d:%s:%s", processName, exeFilePath, parentUIDStr, 0, parentExecFilePathStr, parentFilePath)
	val, err = s.GetLineageInfo(processName, "0", processLineageInfoBucket)
	s.Require().NoError(err)
	assert.Equal(s.T(), expectedProcessLineageInfo, val)
}

func (s *ProcessNetworkTestSuite) TestNetworkFlows() {

	// Server side checks
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

func (s *MissingProcScrapeTestSuite) SetupSuite() {
	_, err := os.Stat("/tmp/fake-proc")
	assert.False(s.T(), os.IsNotExist(err), "Missing fake proc directory")

	s.executor = NewExecutor()
	s.collector = NewCollectorManager(s.executor, s.T().Name())

	// Mount the fake proc directory created by 'create-fake-proc.sh'
	s.collector.Mounts["/host/proc:ro"] = "/tmp/fake-proc"

	err = s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)

	time.Sleep(10 * time.Second)
}

func (s *MissingProcScrapeTestSuite) TestCollectorRunning() {
	collectorRunning, err := s.executor.IsContainerRunning("collector")
	s.Require().NoError(err)
	assert.True(s.T(), collectorRunning, "Collector isn't running")
}

func (s *MissingProcScrapeTestSuite) TearDownSuite() {
	err := s.collector.TearDown()
	s.Require().NoError(err)
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

	s.collector.Env["COLLECTOR_CONFIG"] = `{"logLevel":"debug","turnOffScrape":true,"scrapeInterval":` + strconv.Itoa(s.scrapeInterval) + `,"afterglowPeriod":` + strconv.Itoa(s.afterglowPeriod) + `,"enableAfterglow":` + strconv.FormatBool(s.enableAfterglow) + `}`

	err := s.collector.Setup()
	s.Require().NoError(err)

	err = s.collector.Launch()
	s.Require().NoError(err)

	images := []string{
		"nginx:1.14-alpine",
		"pstauffer/curl:latest",
	}

	for _, image := range images {
		err := s.executor.PullImage(image)
		s.Require().NoError(err)
	}

	time.Sleep(10 * time.Second)

	// invokes default nginx
	containerID, err := s.launchContainer("nginx", "nginx:1.14-alpine")
	s.Require().NoError(err)
	s.serverContainer = containerID[0:12]

	// invokes another container
	containerID, err = s.launchContainer("nginx-curl", "pstauffer/curl:latest", "sleep", "300")
	s.Require().NoError(err)
	s.clientContainer = containerID[0:12]

	s.serverIP, err = s.getIPAddress("nginx")
	s.Require().NoError(err)

	s.serverPort, err = s.getPort("nginx")
	s.Require().NoError(err)
	time.Sleep(30 * time.Second)

	serverAddress := fmt.Sprintf("%s:%s", s.serverIP, s.serverPort)

	for i := 0; i < s.numMetaIter; i++ {
		tickerChannel := make(chan bool)
		t1 := time.NewTicker(time.Duration(s.sleepBetweenCurlTime) * time.Second)
		go func() {
			for {
				select {
				case <-tickerChannel:
					return
				case <-t1.C:
				_, err = s.execContainer("nginx-curl", []string{"curl", serverAddress})
				s.Require().NoError(err)
				}
			}
		}()
		time.Sleep(time.Duration(s.sleepBetweenCurlTime * s.numIter) * time.Second)
		t1.Stop()
		tickerChannel <- true
		time.Sleep(time.Duration(s.sleepBetweenIterations) * time.Second)
	}

	s.clientIP, err = s.getIPAddress("nginx-curl")
	s.Require().NoError(err)

	totalTime := 20 + s.afterglowPeriod
	time.Sleep(time.Duration(totalTime) * time.Second)
	logLines := s.GetLogLines("grpc-server")
	s.observedReports = GetNetworkActivity(logLines)

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
	s.Require().NoError(err)

	benchmarkArgs := []string{
		benchmarkName,
		"--env", "FORCE_TIMES_TO_RUN=1",
		benchmarkImage,
		"phoronix-test-suite", "batch-benchmark", "collector",
	}

	containerID, err := s.launchContainer(benchmarkArgs...)
	s.Require().NoError(err)
	benchmarkContainerID := containerID[0:12]

	_, err = s.waitForContainerToExit(benchmarkName, benchmarkContainerID)
	s.Require().NoError(err)

	benchmarkLogs, err := s.containerLogs("benchmark")
	re := regexp.MustCompile(`Average: ([0-9.]+) Seconds`)
	matches := re.FindSubmatch([]byte(benchmarkLogs))
	if matches != nil {
		fmt.Printf("Benchmark Time: %s\n", matches[1])
		f, err := strconv.ParseFloat(string(matches[1]), 64)
		s.Require().NoError(err)
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
	s.Require().NoError(err)
	args := []string{
		name,
		image,
	}
	containerID, err := s.launchContainer(args...)
	s.Require().NoError(err)
	_, err = s.waitForContainerToExit(name, containerID[0:12])
	s.Require().NoError(err)
}

func (s *IntegrationTestSuiteBase) StartContainerStats() {
	name := "container-stats"
	image := "stackrox/benchmark-collector:stats"
	args := []string{name, "-v", "/var/run/docker.sock:/var/run/docker.sock", image}

	err := s.executor.PullImage(image)
	s.Require().NoError(err)

	_, err = s.launchContainer(args...)
	s.Require().NoError(err)
}

func (s *IntegrationTestSuiteBase) GetLogLines(containerName string) ([]string) {
	logs, err := s.containerLogs(containerName)
	s.Require().NoError(err, containerName + " failure")
	logLines := strings.Split(logs, "\n")
	return logLines
}

func GetNetworkActivity(lines []string) []bool {
	var networkActivity []bool
	inactivePattern := "^Network.*Z$"
	activePattern := "^Network.*nil Timestamp.$"
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
	s.Require().NoError(err)
	defer f.Close()

	_, err = f.WriteString(string(perfJson))
	s.Require().NoError(err)
}
