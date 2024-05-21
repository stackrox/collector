package suites

import (
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/gonum/stat"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/suite"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/executor"
	"github.com/stackrox/collector/integration-tests/pkg/mock_sensor"
	"github.com/stackrox/collector/integration-tests/pkg/types"
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
	executor  executor.Executor
	collector collector.Manager
	sensor    *mock_sensor.MockSensor
	metrics   map[string]float64
	stats     []ContainerStat
	start     time.Time
	stop      time.Time
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
	LoadStartTs      string
	LoadStopTs       string
}

// StartCollector will start the collector container and optionally
// start the MockSensor, if disableGRPC is false.
func (s *IntegrationTestSuiteBase) StartCollector(disableGRPC bool, options *collector.StartupOptions) {
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
		s.Collector().ContainerID())

	if hasHealthCheck && err == nil {
		// Wait for collector to report healthy, includes initial setup and
		// probes loading. It doesn't make sense to wait for very long, limit
		// it to 1 min.
		_, err := s.waitForContainerToBecomeHealthy(
			"collector",
			s.Collector().ContainerID(),
			defaultWaitTickSeconds, 5*time.Minute)
		s.Require().NoError(err)
	} else {
		fmt.Println("No HealthCheck found, do not wait for collector to become healthy")

		// No way to figure out when all the services up and running, skip this
		// phase.
	}

	// wait for the canary process to guarantee collector is started
	selfCheckOk := s.Sensor().WaitProcessesN(
		s.Collector().ContainerID(), 30*time.Second, 1, func() {
			// Self-check process is not going to be sent via GRPC, instead
			// create at least one canary process to make sure everything is
			// fine.
			fmt.Println("Spawn a canary process")
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
func (s *IntegrationTestSuiteBase) Collector() collector.Manager {
	if s.collector == nil {
		s.collector = collector.New(s.Executor(), s.T().Name())
	}
	return s.collector
}

// Executor returns the current executor object, or initializes a new one
// if it is nil.
func (s *IntegrationTestSuiteBase) Executor() executor.Executor {
	if s.executor == nil {
		exec, err := executor.New()
		s.Require().NoError(err)
		s.executor = exec
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
		exists, _ := s.Executor().ContainerExists(executor.ContainerFilter{Name: "collector"})
		if exists {
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

// Convert memory string from docker stats into numeric value in MiB
func Mem2Numeric(value string) (float64, error) {
	size := len(value)

	// Byte units are too short for following logic
	if size < 3 {
		return 0, nil
	}

	numericPart := value[:size-3]
	unitsPart := value[size-3 : size]

	if unitsPart == "MiB" {
		return strconv.ParseFloat(numericPart, 32)
	} else if unitsPart == "GiB" {
		value, err := strconv.ParseFloat(numericPart, 32)
		return value * 1024, err
	} else if unitsPart == "KiB" {
		value, err := strconv.ParseFloat(numericPart, 32)
		return value / 1024, err
	} else {
		return 0, errors.New(fmt.Sprintf("Invalid units, %s", value))
	}
}

func (s *IntegrationTestSuiteBase) PrintContainerStats() {
	cpuStats := map[string][]float64{}
	memStats := map[string][]float64{}

	for _, stat := range s.GetContainerStats() {
		cpuStats[stat.Name] = append(cpuStats[stat.Name], stat.Cpu)

		memValue, err := Mem2Numeric(stat.Mem)
		s.Require().NoError(err)

		memStats[stat.Name] = append(memStats[stat.Name], memValue)
	}

	for name, cpu := range cpuStats {
		s.AddMetric(fmt.Sprintf("%s_cpu_mean", name), stat.Mean(cpu, nil))
		s.AddMetric(fmt.Sprintf("%s_cpu_stddev", name), stat.StdDev(cpu, nil))

		fmt.Printf("CPU: Container %s, Mean %v, StdDev %v\n",
			name, stat.Mean(cpu, nil), stat.StdDev(cpu, nil))
	}

	for name, mem := range memStats {
		s.AddMetric(fmt.Sprintf("%s_mem_mean", name), stat.Mean(mem, nil))
		s.AddMetric(fmt.Sprintf("%s_mem_stddev", name), stat.StdDev(mem, nil))

		fmt.Printf("Mem: Container %s, Mean %v MiB, StdDev %v MiB\n",
			name, stat.Mean(mem, nil), stat.StdDev(mem, nil))
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
		LoadStartTs:      s.start.Format("2006-01-02 15:04:05"),
		LoadStopTs:       s.stop.Format("2006-01-02 15:04:05"),
	}

	perfJson, _ := json.Marshal(perf)
	perfFilename := filepath.Join(config.LogPath(), "perf.json")

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

func (s *IntegrationTestSuiteBase) launchContainer(name string, args ...string) (string, error) {
	cmd := []string{executor.RuntimeCommand, "run", "-d", "--name", name}
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
		executor.RuntimeCommand, "ps", "-qa",
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
				fmt.Printf("Retrying waitForContainerStatus(%s, %s): Error: %v\n",
					containerName, containerID, err)
			}
		case <-timeout:
			fmt.Printf("Timed out waiting for container %s to become %s, elapsed Time: %s\n",
				containerName, filter, time.Since(start))
			return false, fmt.Errorf("Timeout waiting for container %s to become %s after %v",
				containerName, filter, timeoutThreshold)
		case <-tickElapsed:
			fmt.Printf("Waiting for container %s to become %s, elapsed time: %s\n",
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
		executor.RuntimeCommand, "inspect", "-f",
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
	cmd := []string{executor.RuntimeCommand, "exec", containerName}
	cmd = append(cmd, command...)

	return s.Executor().Exec(cmd...)
}

func (s *IntegrationTestSuiteBase) execContainerShellScript(containerName string, shell string, script string, args ...string) (string, error) {
	cmd := []string{executor.RuntimeCommand, "exec", "-i", containerName, shell, "-s"}
	cmd = append(cmd, args...)

	return s.Executor().ExecWithStdin(script, cmd...)
}

func (s *IntegrationTestSuiteBase) cleanupContainers(containers ...string) {
	for _, container := range containers {
		s.Executor().KillContainer(container)
		s.Executor().RemoveContainer(executor.ContainerFilter{Name: container})
	}
}

func (s *IntegrationTestSuiteBase) stopContainers(containers ...string) {
	for _, container := range containers {
		s.Executor().StopContainer(container)
	}
}

func (s *IntegrationTestSuiteBase) removeContainers(containers ...string) {
	for _, container := range containers {
		s.Executor().RemoveContainer(executor.ContainerFilter{Name: container})
	}
}

func (s *IntegrationTestSuiteBase) containerLogs(containerName string) (string, error) {
	return s.Executor().Exec(executor.RuntimeCommand, "logs", containerName)
}

func (s *IntegrationTestSuiteBase) getIPAddress(containerName string) (string, error) {
	args := []string{
		executor.RuntimeCommand,
		"inspect",
		"--format='{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'",
		containerName,
	}

	stdoutStderr, err := s.Executor().Exec(args...)
	return strings.Replace(string(stdoutStderr), "'", "", -1), err
}

func (s *IntegrationTestSuiteBase) getPort(containerName string) (string, error) {
	args := []string{
		executor.RuntimeCommand,
		"inspect",
		"--format='{{json .NetworkSettings.Ports}}'",
		containerName,
	}

	stdoutStderr, err := s.Executor().Exec(args...)
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

func (s *IntegrationTestSuiteBase) StartContainerStats() {
	image := config.Images().QaImageByKey("performance-stats")
	args := []string{"-v", executor.RuntimeSocket + ":/var/run/docker.sock", image}

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
