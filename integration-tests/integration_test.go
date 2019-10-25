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
	processBucket = "Process"
	networkBucket = "Network"
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
	clientPort      string
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

func (s *BenchmarkCollectorTestSuite) SetupSuite() {
	s.executor = NewExecutor()
	s.StartContainerStats()
	s.collector = NewCollectorManager(s.executor)
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
	s.collector = NewCollectorManager(s.executor)

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

	ip, err := s.getIPAddress("nginx")
	require.NoError(s.T(), err)
	s.serverIP = ip

	port, err := s.getPort("nginx")
	require.NoError(s.T(), err)

	s.serverPort = port

	_, err = s.execContainer("nginx-curl", []string{"curl", s.serverIP})
	require.NoError(s.T(), err)

	ip, err = s.getIPAddress("nginx-curl")
	require.NoError(s.T(), err)
	s.clientIP = ip

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

func (s *ProcessNetworkTestSuite) TestNetworkFlows() {

	// Server side checks
	val, err := s.Get(s.serverContainer, networkBucket)
	require.NoError(s.T(), err)
	actualValues := strings.Split(string(val), ":")

	if len(actualValues) < 3 {
		assert.FailNow(s.T(), "serverContainer networkBucket was missing data. ", "val=\"%s\"", val)
	}
	actualServerIP := actualValues[0]
	actualServerPort := actualValues[1]
	actualClientIP := actualValues[2]
	// client port are chosen at random so not checking that

	assert.Equal(s.T(), s.serverIP, actualServerIP)
	assert.Equal(s.T(), s.serverPort, actualServerPort)
	assert.Equal(s.T(), s.clientIP, actualClientIP)

	fmt.Printf("ServerDetails from Bolt: %s %s\n", s.serverContainer, string(val))
	fmt.Printf("ServerDetails from test: %s %s, Port: %s\n", s.serverContainer, s.serverIP, s.serverPort)

	// client side checks
	val, err = s.Get(s.clientContainer, networkBucket)
	actualValues = strings.Split(string(val), ":")
	require.NoError(s.T(), err)

	actualClientIP = actualValues[0]
	actualServerIP = actualValues[2]
	actualServerPort = actualValues[3]

	assert.Equal(s.T(), s.clientIP, actualClientIP)
	assert.Equal(s.T(), s.serverIP, actualServerIP)
	assert.Equal(s.T(), s.serverPort, actualServerPort)

	fmt.Printf("ClientDetails from Bolt: %s %s\n", s.clientContainer, string(val))
	fmt.Printf("ClientDetails from test: %s %s, Port: %s\n", s.clientContainer, s.clientIP, s.clientPort)
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

func (s *IntegrationTestSuiteBase) StartContainerStats() {
	name := "container-stats"
	image := "stackrox/benchmark-collector:stats"
	args := []string{name, "-v", "/var/run/docker.sock:/var/run/docker.sock", image}

	err := s.executor.PullImage(image)
	require.NoError(s.T(), err)

	_, err = s.launchContainer(args...)
	require.NoError(s.T(), err)
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
