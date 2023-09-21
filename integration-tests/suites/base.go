package suites

import (
	"encoding/json"
	"fmt"
	"os"
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/gonum/stat"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/suite"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/config"
	"github.com/stackrox/collector/integration-tests/suites/mock_sensor"
	"github.com/stackrox/collector/integration-tests/suites/types"
)

const (
	processBucket            = "Process"
	processLineageInfoBucket = "LineageInfo"
	networkBucket            = "Network"
	endpointBucket           = "Endpoint"
	parentUIDStr             = "ParentUid"
	parentExecFilePathStr    = "ParentExecFilePath"

	defaultWaitTickSeconds = 30 * time.Second

	nilTimestamp = "(timestamp: nil Timestamp)"
)

type IntegrationTestSuiteBase struct {
	suite.Suite
	executor  common.Executor
	collector *common.CollectorManager
	sensor    *mock_sensor.MockSensor
	metrics   map[string]float64
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
	Timestamp        string
	InstanceType     string
	VmConfig         string
	CollectionMethod string
	Metrics          map[string]float64
	ContainerStats   []ContainerStat
}

// StartCollector will start the collector container and optionally
// start the MockSensor, if disableGRPC is false.
func (s *IntegrationTestSuiteBase) StartCollector(disableGRPC bool) {
	if !disableGRPC {
		s.Sensor().Start()
	}

	s.Require().NoError(s.SetSelinuxPermissiveIfNeeded())
	s.Require().NoError(s.Collector().Setup())
	s.Require().NoError(s.Collector().Launch())

	// wait for self-check process to guarantee collector is started
	s.Sensor().WaitProcessesN(s.Collector().ContainerID, 30*time.Second, 1)
}

// StopCollector will tear down the collector container and stop
// the MockSensor if it was started.
func (s *IntegrationTestSuiteBase) StopCollector() {
	s.Require().NoError(s.collector.TearDown())
	if s.sensor != nil {
		s.sensor.Stop()
	}
}

// Collector returns the current collector object, or initializes a new
// one if it is nil. This function can be used to get the object before
// the container is launched, so that Collector settings can be adjusted
// by individual test suites
func (s *IntegrationTestSuiteBase) Collector() *common.CollectorManager {
	if s.collector == nil {
		s.collector = common.NewCollectorManager(s.Executor(), s.T().Name())
	}
	return s.collector
}

// Executor returns the current executor object, or initializes a new one
// if it is nil.
func (s *IntegrationTestSuiteBase) Executor() common.Executor {
	if s.executor == nil {
		s.executor = common.NewExecutor()
	}
	return s.executor
}

// Sensor returns the current mock sensor object, or initializes a new one
// if it is nil.
func (s *IntegrationTestSuiteBase) Sensor() *mock_sensor.MockSensor {
	if s.sensor == nil {
		s.sensor = mock_sensor.NewMockSensor(s.T().Name())
	}
	return s.sensor
}

// AddMetric wraps access to the metrics map, to avoid nil pointers
// lazy initialization is necessary due to limitations around
// suite setup.
func (s *IntegrationTestSuiteBase) AddMetric(key string, value float64) {
	if s.metrics == nil {
		s.metrics = make(map[string]float64)
	}

	s.metrics[key] = value
}

// SetSelinuxPermissiveIfNeeded will disable SELinux enforcing mode
// on platforms that require it, to enable all integration test components
// to run correctly
func (s *IntegrationTestSuiteBase) SetSelinuxPermissiveIfNeeded() error {
	if s.isSelinuxPermissiveNeeded() {
		return s.setSelinuxPermissive()
	}
	return nil
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
		s.AddMetric(fmt.Sprintf("%s_cpu_mean", name), stat.Mean(cpu, nil))
		s.AddMetric(fmt.Sprintf("%s_cpu_stddev", name), stat.StdDev(cpu, nil))

		fmt.Printf("CPU: Container %s, Mean %v, StdDev %v\n",
			name, stat.Mean(cpu, nil), stat.StdDev(cpu, nil))
	}
}

func (s *IntegrationTestSuiteBase) WritePerfResults(testName string, stats []ContainerStat, metrics map[string]float64) {
	perf := PerformanceResult{
		TestName:         testName,
		Timestamp:        time.Now().Format("2006-01-02 15:04:05"),
		InstanceType:     config.VMInfo().InstanceType,
		VmConfig:         config.VMInfo().Config,
		CollectionMethod: config.CollectionMethod(),
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

func (s *IntegrationTestSuiteBase) AssertProcessInfoEqual(expected, actual types.ProcessInfo) {
	assert := assert.New(s.T())

	assert.Equal(expected.Name, actual.Name)
	assert.Equal(expected.ExePath, actual.ExePath)
	assert.Equal(expected.Uid, actual.Uid)
	assert.Equal(expected.Gid, actual.Gid)
	// Pid is non-deterministic, so just check that it is set
	assert.True(actual.Pid > 0)
	assert.Equal(expected.Args, actual.Args)
}

func (s *IntegrationTestSuiteBase) GetLogLines(containerName string) []string {
	logs, err := s.containerLogs(containerName)
	s.Require().NoError(err, containerName+" failure")
	logLines := strings.Split(logs, "\n")
	return logLines
}

func (s *IntegrationTestSuiteBase) launchContainer(args ...string) (string, error) {
	cmd := []string{common.RuntimeCommand, "run", "-d", "--name"}
	cmd = append(cmd, args...)

	output, err := common.Retry(func() (string, error) {
		return s.Executor().Exec(cmd...)
	})

	outLines := strings.Split(output, "\n")
	return outLines[len(outLines)-1], err
}

func (s *IntegrationTestSuiteBase) waitForContainerToExit(containerName, containerID string, tickSeconds time.Duration) (bool, error) {
	cmd := []string{
		common.RuntimeCommand, "ps", "-qa",
		"--filter", "id=" + containerID,
		"--filter", "status=exited",
	}

	start := time.Now()
	tick := time.Tick(tickSeconds)
	tickElapsed := time.Tick(1 * time.Minute)
	timeout := time.After(30 * time.Minute)
	for {
		select {
		case <-tick:
			output, err := s.Executor().Exec(cmd...)
			outLines := strings.Split(output, "\n")
			lastLine := outLines[len(outLines)-1]
			if lastLine == common.ContainerShortID(containerID) {
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
	cmd := []string{common.RuntimeCommand, "exec", containerName}
	cmd = append(cmd, command...)
	return s.Executor().Exec(cmd...)
}

func (s *IntegrationTestSuiteBase) execContainerShellScript(containerName string, shell string, script string, args ...string) (string, error) {
	cmd := []string{common.RuntimeCommand, "exec", "-i", containerName, shell, "-s"}
	cmd = append(cmd, args...)
	return s.Executor().ExecWithStdin(script, cmd...)
}

func (s *IntegrationTestSuiteBase) cleanupContainer(containers []string) {
	for _, container := range containers {
		s.Executor().Exec(common.RuntimeCommand, "kill", container)
		s.Executor().Exec(common.RuntimeCommand, "rm", container)
	}
}

func (s *IntegrationTestSuiteBase) stopContainers(containers ...string) {
	for _, container := range containers {
		s.Executor().Exec(common.RuntimeCommand, "stop", "-t", config.StopTimeout(), container)
	}
}

func (s *IntegrationTestSuiteBase) removeContainers(containers ...string) {
	for _, container := range containers {
		s.Executor().Exec(common.RuntimeCommand, "rm", container)
	}
}

func (s *IntegrationTestSuiteBase) containerLogs(containerName string) (string, error) {
	return s.Executor().Exec(common.RuntimeCommand, "logs", containerName)
}

func (s *IntegrationTestSuiteBase) getIPAddress(containerName string) (string, error) {
	stdoutStderr, err := s.Executor().Exec(common.RuntimeCommand, "inspect", "--format='{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'", containerName)
	return strings.Replace(string(stdoutStderr), "'", "", -1), err
}

func (s *IntegrationTestSuiteBase) getPort(containerName string) (string, error) {
	stdoutStderr, err := s.Executor().Exec(common.RuntimeCommand, "inspect", "--format='{{json .NetworkSettings.Ports}}'", containerName)
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

func (s *IntegrationTestSuiteBase) RunCollectorBenchmark() {
	benchmarkName := "benchmark"
	benchmarkImage := config.Images().QaImageByKey("performance-phoronix")

	err := s.Executor().PullImage(benchmarkImage)
	s.Require().NoError(err)

	benchmarkArgs := []string{
		benchmarkName,
		"--env", "FORCE_TIMES_TO_RUN=1",
		benchmarkImage,
		"batch-benchmark", "collector",
	}

	containerID, err := s.launchContainer(benchmarkArgs...)
	s.Require().NoError(err)

	_, err = s.waitForContainerToExit(benchmarkName, containerID, defaultWaitTickSeconds)
	s.Require().NoError(err)

	benchmarkLogs, err := s.containerLogs("benchmark")
	re := regexp.MustCompile(`Average: ([0-9.]+) Seconds`)
	matches := re.FindSubmatch([]byte(benchmarkLogs))
	if matches != nil {
		fmt.Printf("Benchmark Time: %s\n", matches[1])
		f, err := strconv.ParseFloat(string(matches[1]), 64)
		s.Require().NoError(err)
		s.AddMetric("hackbench_avg_time", f)
	} else {
		fmt.Printf("Benchmark Time: Not found! Logs: %s\n", benchmarkLogs)
		assert.FailNow(s.T(), "Benchmark Time not found")
	}
}

func (s *IntegrationTestSuiteBase) RunImageWithJSONLabels() {
	name := "jsonlabel"
	image := config.Images().QaImageByKey("performance-json-label")
	err := s.Executor().PullImage(image)
	s.Require().NoError(err)
	args := []string{
		name,
		image,
	}
	containerID, err := s.launchContainer(args...)
	s.Require().NoError(err)
	_, err = s.waitForContainerToExit(name, containerID, defaultWaitTickSeconds)
	s.Require().NoError(err)
}

func (s *IntegrationTestSuiteBase) StartContainerStats() {
	name := "container-stats"
	image := config.Images().QaImageByKey("performance-stats")
	args := []string{name, "-v", common.RuntimeSocket + ":/var/run/docker.sock", image}

	err := s.Executor().PullImage(image)
	s.Require().NoError(err)

	_, err = s.launchContainer(args...)
	s.Require().NoError(err)
}

func (s *IntegrationTestSuiteBase) waitForFileToBeDeleted(file string) error {
	timer := time.After(10 * time.Second)
	ticker := time.NewTicker(time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-timer:
			return fmt.Errorf("Timed out waiting for %s to be deleted", file)
		case <-ticker.C:
			if config.HostInfo().Kind == "local" {
				if _, err := os.Stat(file); os.IsNotExist(err) {
					return nil
				}
			} else {
				output, _ := s.Executor().Exec("stat", file)
				if strings.Contains(output, "No such file or directory") {
					return nil
				}
			}
		}
	}

	return nil
}

// isSelinuxPermissiveNeeded returns whether or not a given VM requires
// SELinux permissive mode. e.g. rhel, or fedora-coreos
func (s *IntegrationTestSuiteBase) isSelinuxPermissiveNeeded() bool {
	vmType := config.VMInfo().Config
	if strings.Contains(vmType, "coreos") || strings.Contains(vmType, "rhcos") {
		return true
	}
	if strings.Contains(vmType, "rhel-7") {
		collectionMethod := config.CollectionMethod()
		if collectionMethod == "ebpf" {
			return true
		}
	}
	return false
}

// setSelinuxPermissive sets the VM's SELinux mode to permissive
func (s *IntegrationTestSuiteBase) setSelinuxPermissive() error {
	cmd := []string{"sudo", "setenforce", "0"}
	_, err := s.Executor().Exec(cmd...)
	if err != nil {
		fmt.Printf("Error: Unable to set SELinux to permissive. %v\n", err)
	}
	return err
}
