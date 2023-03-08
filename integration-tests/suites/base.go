package suites

import (
	"encoding/json"
	"fmt"
	"os"
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/boltdb/bolt"
	"github.com/gonum/stat"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/suite"

	"github.com/stackrox/collector/integration-tests/suites/common"
)

const (
	processBucket            = "Process"
	processLineageInfoBucket = "LineageInfo"
	networkBucket            = "Network"
	endpointBucket           = "Endpoint"
	parentUIDStr             = "ParentUid"
	parentExecFilePathStr    = "ParentExecFilePath"

	defaultWaitTickSeconds = 30 * time.Second

	// defaultStopTimeoutSeconds is the amount of time to wait for a container
	// to stop before forcibly killing it. It needs to be a string because it
	// is passed directly to the docker command via the executor.
	//
	// 10 seconds is the default for docker stop when not providing a timeout
	// argument. It is kept the same here to avoid changing behavior by default.
	defaultStopTimeoutSeconds = "10"

	nilTimestamp = "(timestamp: nil Timestamp)"
)

type IntegrationTestSuiteBase struct {
	suite.Suite
	db        *bolt.DB
	executor  common.Executor
	collector *common.CollectorManager
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
		Timestamp:        time.Now().Format("2006-01-02 15:04:05"),
		InstanceType:     common.ReadEnvVarWithDefault("VM_INSTANCE_TYPE", "default"),
		VmConfig:         common.ReadEnvVarWithDefault("VM_CONFIG", "default"),
		CollectionMethod: common.ReadEnvVarWithDefault("COLLECTION_METHOD", "kernel_module"),
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

func (s *IntegrationTestSuiteBase) AssertProcessInfoEqual(expected, actual common.ProcessInfo) {
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
		return s.executor.Exec(cmd...)
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
			output, err := s.executor.Exec(cmd...)
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
	return s.executor.Exec(cmd...)
}

func (s *IntegrationTestSuiteBase) execContainerShellScript(containerName string, script string, args ...string) (string, error) {
	cmd := []string{"docker", "exec", "-i", containerName, "/bin/sh", "-s"}
	cmd = append(cmd, args...)
	return s.executor.ExecWithStdin(script, cmd...)
}

func (s *IntegrationTestSuiteBase) cleanupContainer(containers []string) {
	for _, container := range containers {
		s.executor.Exec(common.RuntimeCommand, "kill", container)
		s.executor.Exec(common.RuntimeCommand, "rm", container)
	}
}

func (s *IntegrationTestSuiteBase) stopContainers(containers ...string) {
	timeout := common.ReadEnvVarWithDefault("STOP_TIMEOUT", defaultStopTimeoutSeconds)
	for _, container := range containers {
		s.executor.Exec(common.RuntimeCommand, "stop", "-t", timeout, container)
	}
}

func (s *IntegrationTestSuiteBase) removeContainers(containers ...string) {
	for _, container := range containers {
		s.executor.Exec(common.RuntimeCommand, "rm", container)
	}
}

func (s *IntegrationTestSuiteBase) containerLogs(containerName string) (string, error) {
	return s.executor.Exec(common.RuntimeCommand, "logs", containerName)
}

func (s *IntegrationTestSuiteBase) getIPAddress(containerName string) (string, error) {
	stdoutStderr, err := s.executor.Exec(common.RuntimeCommand, "inspect", "--format='{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'", containerName)
	return strings.Replace(string(stdoutStderr), "'", "", -1), err
}

func (s *IntegrationTestSuiteBase) getPort(containerName string) (string, error) {
	stdoutStderr, err := s.executor.Exec(common.RuntimeCommand, "inspect", "--format='{{json .NetworkSettings.Ports}}'", containerName)
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

func (s *IntegrationTestSuiteBase) GetProcesses(containerID string) ([]common.ProcessInfo, error) {
	if s.db == nil {
		return nil, fmt.Errorf("Db %v is nil", s.db)
	}

	processes := make([]common.ProcessInfo, 0)
	err := s.db.View(func(tx *bolt.Tx) error {
		process := tx.Bucket([]byte(processBucket))
		if process == nil {
			return fmt.Errorf("Process bucket was not found!")
		}
		container := process.Bucket([]byte(containerID))
		if container == nil {
			return fmt.Errorf("Container bucket %s not found!", containerID)
		}

		return container.ForEach(func(k, v []byte) error {
			pinfo, err := common.NewProcessInfo(string(v))
			if err != nil {
				return err
			}

			if strings.HasPrefix(pinfo.ExePath, "/proc/self") {
				//
				// There exists a potential race condition for the driver
				// to capture very early container process events.
				//
				// This is known in falco, and somewhat documented here:
				//     https://github.com/falcosecurity/falco/blob/555bf9971cdb79318917949a5e5f9bab5293b5e2/rules/falco_rules.yaml#L1961
				//
				// It is also filtered in sensor here:
				//    https://github.com/stackrox/stackrox/blob/4d3fb539547d1935a35040e4a4e8c258a53a92e4/sensor/common/signal/signal_service.go#L90
				//
				// Further details can be found here https://issues.redhat.com/browse/ROX-11544
				//
				return nil
			}

			processes = append(processes, *pinfo)
			return nil
		})
	})

	if err != nil {
		return nil, err
	}

	return processes, nil
}

func (s *IntegrationTestSuiteBase) GetEndpoints(containerID string) ([]common.EndpointInfo, error) {
	if s.db == nil {
		return nil, fmt.Errorf("Db %v is nil", s.db)
	}

	endpoints := make([]common.EndpointInfo, 0)
	err := s.db.View(func(tx *bolt.Tx) error {
		endpoint := tx.Bucket([]byte(endpointBucket))
		if endpoint == nil {
			return fmt.Errorf("Endpoint bucket was not found!")
		}
		container := endpoint.Bucket([]byte(containerID))
		if container == nil {
			return fmt.Errorf("Container bucket %s not found!", containerID)
		}

		return container.ForEach(func(k, v []byte) error {
			einfo, err := common.NewEndpointInfo(string(v))
			if err != nil {
				return err
			}

			endpoints = append(endpoints, *einfo)
			return nil
		})
	})

	if err != nil {
		return nil, err
	}

	return endpoints, nil
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
	benchmarkImage := common.QaImage("quay.io/rhacs-eng/collector-performance", "phoronix")

	err := s.executor.PullImage(benchmarkImage)
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
		s.metrics["hackbench_avg_time"] = f
	} else {
		fmt.Printf("Benchmark Time: Not found! Logs: %s\n", benchmarkLogs)
		assert.FailNow(s.T(), "Benchmark Time not found")
	}
}

func (s *IntegrationTestSuiteBase) RunImageWithJSONLabels() {
	name := "jsonlabel"
	image := common.QaImage("quay.io/rhacs-eng/collector-performance", "json-label")
	err := s.executor.PullImage(image)
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
	image := common.QaImage("quay.io/rhacs-eng/collector-performance", "stats")
	args := []string{name, "-v", common.RuntimeSocket + ":/var/run/docker.sock", image}

	err := s.executor.PullImage(image)
	s.Require().NoError(err)

	_, err = s.launchContainer(args...)
	s.Require().NoError(err)
}

func (s *IntegrationTestSuiteBase) waitForFileToBeDeleted(file string) error {
	count := 0
	maxCount := 10

	output, _ := s.executor.Exec("stat", file, "2>&1")
	fmt.Println(output)
	for !strings.Contains(output, "No such file or directory") {
		time.Sleep(1 * time.Second)
		count += 1
		if count == maxCount {
			return fmt.Errorf("Timed out waiting for %s to be deleted", file)
		}
		output, _ = s.executor.Exec("stat", file, "2>&1")
	}

	return nil
}
