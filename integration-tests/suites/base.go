package suites

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/gonum/stat"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/suite"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/executor"
	"github.com/stackrox/collector/integration-tests/pkg/log"
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

	defaultWaitTickSeconds = 1 * time.Second
)

type IntegrationTestSuiteBase struct {
	suite.Suite
	executor  executor.Executor
	collector collector.Manager
	sensor    *mock_sensor.MockSensor
	metrics   map[string]float64

	stats       []executor.ContainerStat
	statsCtx    context.Context
	statsCancel context.CancelFunc

	start time.Time
	stop  time.Time
}

type PerformanceResult struct {
	TestName         string
	Timestamp        string
	VmConfig         string
	CollectionMethod string
	Metrics          map[string]float64
	ContainerStats   []executor.ContainerStat
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
			defaultWaitTickSeconds, 1*time.Minute)
		s.Require().NoError(err)
	} else {
		log.Error("No HealthCheck found, do not wait for collector to become healthy")

		// No way to figure out when all the services up and running, skip this
		// phase.
	}

	// wait for the canary process to guarantee collector is started
	selfCheckOk := s.Sensor().WaitProcessesN(
		s.Collector().ContainerID(), 30*time.Second, 1, func() {
			// Self-check process is not going to be sent via GRPC, instead
			// create at least one canary process to make sure everything is
			// fine.
			log.Info("Spawn a canary process")
			_, err = s.execContainer("collector", []string{"echo"}, false)
			s.Require().NoError(err)
		})
	s.Require().True(selfCheckOk)

	// The collector must be running by now, initiate collecting stats
	s.StartContainerStats()
}

// StopCollector will tear down the collector container and stop
// the MockSensor if it was started.
func (s *IntegrationTestSuiteBase) StopCollector() {
	// Stop stats collection first
	s.statsCancel()
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
	} else if s.collector.GetTestName() != s.T().Name() {
		// Otherwise the test name does not get set when subsequent tests are run
		s.collector.SetTestName(s.T().Name())
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
	} else {
		s.sensor.SetTestName(s.T().Name())
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
		s.cleanupContainers(containers...)

		// StopCollector is safe when collector isn't running, but the container must exist.
		// This will ensure that logs are still written even when test setup fails
		exists, _ := s.Executor().ContainerExists(executor.ContainerFilter{Name: "collector"})
		if exists {
			s.StopCollector()
		}
	})
}

func (s *IntegrationTestSuiteBase) SnapshotContainerStats() {
	if s.stats == nil {
		s.stats = make([]executor.ContainerStat, 0)
	}

	if s.statsCtx == nil {
		s.statsCtx, s.statsCancel = context.WithCancel(context.Background())
	}

	stat, err := s.Executor().GetContainerStats(s.statsCtx, s.Collector().ContainerID())

	// The stats context will be canceled at the end of the test. If it happens
	// while the request is in flight, ignore the error.
	if err != nil {
		if errors.Is(err, context.Canceled) {
			return
		}

		// CRI client returns a GRPC Status instead of an error, making it
		// harder to unwrap. Compare the code instead.
		var grpcStatus interface{ GRPCStatus() *status.Status }

		if errors.As(err, &grpcStatus) {
			code := grpcStatus.GRPCStatus().Code()

			if code == codes.Canceled {
				return
			}
		}

		assert.FailNowf(s.T(), "fail to get container stats", "%v", err)
	}

	if stat == nil {
		log.Info("No ContainerStats found")
		return
	}

	s.stats = append(s.stats, *stat)
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

	for _, stat := range s.stats {
		cpuStats[stat.Name] = append(cpuStats[stat.Name], float64(stat.Cpu))
		memStats[stat.Name] = append(memStats[stat.Name], float64(stat.Mem))
	}

	for name, cpu := range cpuStats {
		s.AddMetric(fmt.Sprintf("%s_cpu_mean", name), stat.Mean(cpu, nil))
		s.AddMetric(fmt.Sprintf("%s_cpu_stddev", name), stat.StdDev(cpu, nil))

		log.Trace("CPU: Container %s, Mean %v, StdDev %v",
			name, stat.Mean(cpu, nil), stat.StdDev(cpu, nil))
	}

	for name, mem := range memStats {
		s.AddMetric(fmt.Sprintf("%s_mem_mean", name), stat.Mean(mem, nil))
		s.AddMetric(fmt.Sprintf("%s_mem_stddev", name), stat.StdDev(mem, nil))

		log.Trace("Mem: Container %s, Mean %v MiB, StdDev %v MiB",
			name, stat.Mean(mem, nil), stat.StdDev(mem, nil))
	}
}

func (s *IntegrationTestSuiteBase) WritePerfResults() {
	s.PrintContainerStats()

	perf := PerformanceResult{
		TestName:         s.T().Name(),
		Timestamp:        time.Now().Format("2006-01-02 15:04:05"),
		VmConfig:         config.VMInfo().Config,
		CollectionMethod: config.CollectionMethod(),
		Metrics:          s.metrics,
		ContainerStats:   s.stats,
		LoadStartTs:      s.start.Format("2006-01-02 15:04:05"),
		LoadStopTs:       s.stop.Format("2006-01-02 15:04:05"),
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
			found, err := s.Executor().IsContainerFoundFiltered(containerID, filter)
			if err != nil {
				log.Error("Retrying waitForContainerStatus(%s, %s): Error: %v\n",
					containerName, containerID, err)
			}
			if found {
				return true, nil
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

	healthcheck, err := s.Executor().GetContainerHealthCheck(containerName)
	if err != nil {
		return false, err
	}

	// If doesn't contain an expected command, do not consider it to be a valid
	// health check
	if strings.Contains(healthcheck, "CMD-SHELL /usr/local/bin/status-check.sh") {
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

func (s *IntegrationTestSuiteBase) execContainer(containerName string, command []string, detach bool) (string, error) {
	return s.Executor().ExecContainer(&executor.ExecOptions{
		ContainerName: containerName,
		Command:       command,
		Detach:        detach,
	})
}

func (s *IntegrationTestSuiteBase) cleanupContainers(containers ...string) {
	for _, container := range containers {
		exists, _ := s.Executor().ContainerExists(executor.ContainerFilter{Name: container})
		if exists {
			s.Executor().KillContainer(container)
			s.Executor().CaptureLogs(s.T().Name(), container)
			s.Executor().RemoveContainer(executor.ContainerFilter{Name: container})
		}
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

func (s *IntegrationTestSuiteBase) containerLogs(containerName string) (executor.ContainerLogs, error) {
	return s.Executor().GetContainerLogs(containerName)
}

func (s *IntegrationTestSuiteBase) getIPAddress(containerName string) (string, error) {
	return s.Executor().GetContainerIP(containerName)
}

// unit16 makes more sense for ports, but sensor.NetworkConnection uses uint32 for
// ports, so uint32 is used for ports.
func (s *IntegrationTestSuiteBase) getPort(containerName string) (uint32, error) {
	portStr, err := s.Executor().GetContainerPort(containerName)
	port, _ := strconv.ParseUint(portStr, 10, 32)
	return uint32(port), err
}

func (s *IntegrationTestSuiteBase) StartContainerStats() {
	tick := time.Tick(defaultWaitTickSeconds)
	s.statsCtx, s.statsCancel = context.WithCancel(context.Background())

	go func() {
		for {
			select {
			case <-tick:
				s.SnapshotContainerStats()
			case <-s.statsCtx.Done():
				return
			}
		}
	}()
}

func (s *IntegrationTestSuiteBase) execShellCommand(command string) error {
	log.Info("[exec] %s", command)
	cmd := exec.Command("sh", "-c", command)
	_, err := cmd.Output()
	if err != nil {
		log.Info("[exec]: %v", err)
	}
	return err
}

func (s *IntegrationTestSuiteBase) createDirectory(dir string) {
	if _, err := os.Stat(dir); err == nil {
		return
	}
	err := os.Mkdir(dir, os.ModePerm)
	s.Require().NoError(err)
}

func (s *IntegrationTestSuiteBase) deleteDirectory(dir string) {
	_, err := os.Stat(dir)
	if os.IsNotExist(err) {
		return
	}

	err = os.RemoveAll(dir)
	s.Require().NoError(err)
}

func (s *IntegrationTestSuiteBase) deleteFile(file string) {
	if _, err := os.Stat(file); os.IsNotExist(err) {
		return
	}
	err := os.Remove(file)
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
			if config.HostInfo().IsLocal() {
				if _, err := os.Stat(file); os.IsNotExist(err) {
					return nil
				}
			} else {
				return errors.New("waitForFileToBeDeleted only supported local hosts")
			}
		}
	}
}
