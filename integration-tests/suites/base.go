package suites

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/gonum/stat"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/suite"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/config"
	"github.com/stackrox/collector/integration-tests/suites/log"
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

	containerStatsName = "container-stats"

	defaultWaitTickSeconds = 5 * time.Second
)

type IntegrationTestSuiteBase struct {
	suite.Suite
	executor  common.Executor
	collector *common.CollectorManager
	sensor    *mock_sensor.MockSensor
	metrics   map[string]float64
	stats     []ContainerStat
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
func (s *IntegrationTestSuiteBase) StartCollector(disableGRPC bool, options *common.CollectorStartupOptions) {
	if !disableGRPC {
		s.Sensor().Start()
	}

	s.Require().NoError(s.Collector().Setup(options))
	s.Require().NoError(s.Collector().Launch())

	// Verify if the image we test has a health check. There are some CI
	// configurations, where it's not the case. If something went wrong and we
	// get an error, treat it as no health check was found for the sake of
	// robustness.
	hasHealthCheck, err := s.findContainerHealthCheck("collector",
		s.Collector().ContainerID)

	if hasHealthCheck && err == nil {
		// Wait for collector to report healthy, includes initial setup and
		// probes loading. It doesn't make sense to wait for very long, limit
		// it to 1 min.
		_, err := s.waitForContainerToBecomeHealthy(
			"collector",
			s.Collector().ContainerID,
			defaultWaitTickSeconds, 5*time.Minute)
		s.Require().NoError(err)
	} else {
		log.Error("No HealthCheck found, do not wait for collector to become healthy")

		// No way to figure out when all the services up and running, skip this
		// phase.
	}

	// wait for the canary process to guarantee collector is started
	selfCheckOk := s.Sensor().WaitProcessesN(
		s.Collector().ContainerID, 30*time.Second, 1, func() {
			// Self-check process is not going to be sent via GRPC, instead
			// create at least one canary process to make sure everything is
			// fine.
			log.Log("Spawn a canary process")
			_, err = s.execContainer("collector", []string{"echo"})
			s.Require().NoError(err)
		})
	s.Require().True(selfCheckOk)
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

// RegisterCleanup registers a cleanup function with the testing structures,
// to cleanup all containers started by a test / suite (provided as args)
func (s *IntegrationTestSuiteBase) RegisterCleanup(containers ...string) {
	s.T().Cleanup(func() {
		// If the test is successful, this clean up function is still run
		// but everything should be clean already, so this should not fail
		// if resources are already gone.
		containers = append(containers, containerStatsName)
		s.cleanupContainers(containers...)
		// StopCollector is safe when collector isn't running, but the container must exist.
		// This will ensure that logs are still written even when test setup fails
		if exists, _ := s.Executor().ContainerExists("collector"); exists {
			s.StopCollector()
		}
	})
}

func (s *IntegrationTestSuiteBase) GetContainerStats() []ContainerStat {
	if s.stats == nil {
		s.stats = make([]ContainerStat, 0)

		logs, err := s.containerLogs(containerStatsName)
		if err != nil {
			assert.FailNow(s.T(), "container-stats failure")
			return nil
		}

		logLines := strings.Split(logs, "\n")
		for _, line := range logLines {
			var stat ContainerStat

			_ = json.Unmarshal([]byte(line), &stat)

			s.stats = append(s.stats, stat)
		}

		s.cleanupContainers(containerStatsName)
	}

	return s.stats
}

func (s *IntegrationTestSuiteBase) PrintContainerStats() {
	cpuStats := map[string][]float64{}

	for _, stat := range s.GetContainerStats() {
		cpuStats[stat.Name] = append(cpuStats[stat.Name], stat.Cpu)
	}

	for name, cpu := range cpuStats {
		s.AddMetric(fmt.Sprintf("%s_cpu_mean", name), stat.Mean(cpu, nil))
		s.AddMetric(fmt.Sprintf("%s_cpu_stddev", name), stat.StdDev(cpu, nil))

		log.Log("CPU: Container %s, Mean %v, StdDev %v\n",
			name, stat.Mean(cpu, nil), stat.StdDev(cpu, nil))
	}
}

func (s *IntegrationTestSuiteBase) WritePerfResults() {
	s.PrintContainerStats()

	perf := PerformanceResult{
		TestName:         s.T().Name(),
		Timestamp:        time.Now().Format("2006-01-02 15:04:05"),
		InstanceType:     config.VMInfo().InstanceType,
		VmConfig:         config.VMInfo().Config,
		CollectionMethod: config.CollectionMethod(),
		Metrics:          s.metrics,
		ContainerStats:   s.GetContainerStats(),
	}

	perfJson, _ := json.Marshal(perf)
	perfFilename := filepath.Join(config.LogPath(), "perf.json")

	log.Info("Writing %s\n", perfFilename)
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

func (s *IntegrationTestSuiteBase) launchContainer(name string, args ...string) (string, error) {
	cmd := []string{common.RuntimeCommand, "run", "-d", "--name", name}
	cmd = append(cmd, args...)

	output, err := s.Executor().Exec(cmd...)

	outLines := strings.Split(output, "\n")
	return outLines[len(outLines)-1], err
}

// Wait for a container to become a certain status.
//   - tickSeconds -- how often to check for the status
//   - timeoutThreshold -- the overall time limit for waiting,
//     defaulting to 30 min if zero
//   - filter -- description of the desired status
func (s *IntegrationTestSuiteBase) waitForContainerStatus(
	containerName string,
	containerID string,
	tickSeconds time.Duration,
	timeoutThreshold time.Duration,
	filter string) (bool, error) {

	cmd := []string{
		common.RuntimeCommand, "ps", "-qa",
		"--filter", "id=" + containerID,
		"--filter", filter,
	}

	start := time.Now()
	tick := time.Tick(tickSeconds)
	tickElapsed := time.Tick(1 * time.Minute)

	if timeoutThreshold == 0 {
		timeoutThreshold = 30 * time.Minute
	}
	timeout := time.After(timeoutThreshold)

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
				log.Error("Retrying waitForContainerStatus(%s, %s): Error: %v\n",
					containerName, containerID, err)
			}
		case <-timeout:
			log.Error("Timed out waiting for container %s to become %s, elapsed Time: %s\n",
				containerName, filter, time.Since(start))
			return false, fmt.Errorf("Timeout waiting for container %s to become %s after %v",
				containerName, filter, timeoutThreshold)
		case <-tickElapsed:
			log.Error("Waiting for container %s to become %s, elapsed time: %s\n",
				containerName, filter, time.Since(start))
		}
	}
}

// Find a HealthCheck section in the specified container and verify it's what
// we expect. This function would be used to wait until the health check
// reports healthy, so be conservative and report true only if absolutely
// certain.
func (s *IntegrationTestSuiteBase) findContainerHealthCheck(
	containerName string,
	containerID string) (bool, error) {

	cmd := []string{
		common.RuntimeCommand, "inspect", "-f",
		"'{{ .Config.Healthcheck }}'", containerID,
	}

	output, err := s.Executor().Exec(cmd...)
	if err != nil {
		return false, err
	}

	outLines := strings.Split(output, "\n")
	lastLine := outLines[len(outLines)-1]

	// Clearly no HealthCheck section
	if lastLine == "<nil>" {
		return false, nil
	}

	// If doesn't contain an expected command, do not consider it to be a valid
	// health check
	if strings.Contains(lastLine, "CMD-SHELL /usr/local/bin/status-check.sh") {
		return true, nil
	} else {
		return false, nil
	}
}

func (s *IntegrationTestSuiteBase) waitForContainerToBecomeHealthy(
	containerName string,
	containerID string,
	tickSeconds time.Duration,
	timeoutThreshold time.Duration) (bool, error) {

	return s.waitForContainerStatus(containerName, containerID, tickSeconds,
		timeoutThreshold, "health=healthy")
}

func (s *IntegrationTestSuiteBase) waitForContainerToExit(
	containerName string,
	containerID string,
	tickSeconds time.Duration,
	timeoutThreshold time.Duration) (bool, error) {

	return s.waitForContainerStatus(containerName, containerID, tickSeconds,
		timeoutThreshold, "status=exited")
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

func (s *IntegrationTestSuiteBase) cleanupContainers(containers ...string) {
	log.Debug("Killing and removing containers %v\n", containers)
	for _, container := range containers {
		s.Executor().KillContainer(container)
		s.Executor().RemoveContainer(container)
	}
}

func (s *IntegrationTestSuiteBase) stopContainers(containers ...string) {
	log.Debug("Stopping containers %v\n", containers)
	for _, container := range containers {
		s.Executor().StopContainer(container)
	}
}

func (s *IntegrationTestSuiteBase) removeContainers(containers ...string) {
	log.Debug("Removing containers %v\n", containers)
	for _, container := range containers {
		s.Executor().RemoveContainer(container)
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
		"--env", "FORCE_TIMES_TO_RUN=1",
		benchmarkImage,
		"batch-benchmark", "collector",
	}

	containerID, err := s.launchContainer(benchmarkName, benchmarkArgs...)
	s.Require().NoError(err)

	_, err = s.waitForContainerToExit(benchmarkName, containerID, defaultWaitTickSeconds, 0)
	s.Require().NoError(err)

	benchmarkLogs, err := s.containerLogs("benchmark")
	re := regexp.MustCompile(`Average: ([0-9.]+) Seconds`)
	matches := re.FindSubmatch([]byte(benchmarkLogs))
	if matches != nil {
		log.Log("Benchmark Time: %s\n", matches[1])
		f, err := strconv.ParseFloat(string(matches[1]), 64)
		s.Require().NoError(err)
		s.AddMetric("hackbench_avg_time", f)
	} else {
		log.Error("Benchmark Time: Not found! Logs: %s\n", benchmarkLogs)
		assert.FailNow(s.T(), "Benchmark Time not found")
	}
}

func (s *IntegrationTestSuiteBase) StartContainerStats() {
	image := config.Images().QaImageByKey("performance-stats")
	args := []string{"-v", common.RuntimeSocket + ":/var/run/docker.sock", image}

	err := s.Executor().PullImage(image)
	s.Require().NoError(err)

	_, err = s.launchContainer(containerStatsName, args...)
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
}
