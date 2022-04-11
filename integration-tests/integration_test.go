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

	defaultWaitTickSeconds = 30 * time.Second

	// defaultStopTimeoutSeconds is the amount of time to wait for a container
	// to stop before forcibly killing it. It needs to be a string because it
	// is passed directly to the docker command via the executor.
	//
	// 10 seconds is the default for docker stop when not providing a timeout
	// argument. It is kept the same here to avoid changing behavior by default.
	defaultStopTimeoutSeconds = "10"
)

func TestCollectorGRPC(t *testing.T) {
	suite.Run(t, new(ProcessNetworkTestSuite))
	suite.Run(t, new(BenchmarkCollectorTestSuite))
}

type IntegrationTestSuiteBase struct {
	suite.Suite
	db        *bolt.DB
	executor  Executor
	docker    Docker
	collector *collectorManager
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
	VmConfig         string
	CollectionMethod string
	Metrics          map[string]float64
	ContainerStats   []ContainerStat
}

func (s *IntegrationTestSuiteBase) launchContainer(name string, args ...string) (string, error) {
	cmd := []string{"-d"}
	cmd = append(cmd, args...)
	output, err := s.docker.Run(name, cmd...)
	outLines := strings.Split(output, "\n")
	return outLines[len(outLines)-1], err
}

func (s *IntegrationTestSuiteBase) waitForContainerToExit(containerName, containerID string, tickSeconds time.Duration) (bool, error) {
	cmdArgs := []string{
		"-qa",
		"--filter", "id=" + containerID,
		"--filter", "status=exited",
	}

	start := time.Now()
	tick := time.Tick(tickSeconds)
	tickElapsed := time.Tick(1 * time.Minute)
	timeout := time.After(15 * time.Minute)
	for {
		select {
		case <-tick:
			output, err := s.docker.PS(cmdArgs...)
			outLines := strings.Split(output, "\n")
			lastLine := outLines[len(outLines)-1]
			if lastLine == containerShortID(containerID) {
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

func (s *IntegrationTestSuiteBase) execContainer(name string, command []string) (string, error) {
	return s.docker.Exec(name, command...)
}

// cleanupContainers will kill and remove all containers given as arguments.
// this can contain container names or ids
func (s *IntegrationTestSuiteBase) cleanupContainers(containers ...string) {
	s.docker.Kill(containers...)
	s.docker.Remove(containers...)
}

func (s *IntegrationTestSuiteBase) stopContainers(containers ...string) {
	timeout, err := strconv.Atoi(ReadEnvVarWithDefault("STOP_TIMEOUT", defaultStopTimeoutSeconds))
	s.Require().NoError(err)
	s.docker.Stop(time.Duration(timeout)*time.Second, containers...)
}

func (s *IntegrationTestSuiteBase) removeContainers(containers ...string) {
	s.docker.Remove(containers...)
}

func (s *IntegrationTestSuiteBase) containerLogs(name string) (string, error) {
	return s.docker.Logs(name)
}

func (s *IntegrationTestSuiteBase) getIPAddress(name string) (string, error) {
	stdoutStderr, err := s.docker.Inspect(name, "--format='{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'")
	return strings.Replace(string(stdoutStderr), "'", "", -1), err
}

func (s *IntegrationTestSuiteBase) getPort(name string) (string, error) {
	stdoutStderr, err := s.docker.Inspect(name, "--format='{{json .NetworkSettings.Ports}}'")
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

	err := s.docker.Pull(benchmarkImage)
	s.Require().NoError(err)

	benchmarkArgs := []string{
		"--env", "FORCE_TIMES_TO_RUN=1",
		benchmarkImage,
		"phoronix-test-suite", "batch-benchmark", "collector",
	}

	containerID, err := s.launchContainer(benchmarkName, benchmarkArgs...)
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
	image := "stackrox/benchmark-collector:json-label"
	err := s.docker.Pull(image)
	s.Require().NoError(err)
	containerID, err := s.launchContainer(name, image)
	s.Require().NoError(err)
	_, err = s.waitForContainerToExit(name, containerID, defaultWaitTickSeconds)
	s.Require().NoError(err)
}

func (s *IntegrationTestSuiteBase) StartContainerStats() {
	name := "container-stats"
	image := "stackrox/benchmark-collector:stats"
	args := []string{"-v", "/var/run/docker.sock:/var/run/docker.sock", image}

	err := s.docker.Pull(image)
	s.Require().NoError(err)

	_, err = s.launchContainer(name, args...)
	s.Require().NoError(err)
}

func (s *IntegrationTestSuiteBase) GetLogLines(containerName string) []string {
	logs, err := s.containerLogs(containerName)
	s.Require().NoError(err, containerName+" failure")
	logLines := strings.Split(logs, "\n")
	return logLines
}

func GetNetworkActivity(lines []string, serverAddress string) []bool {
	var networkActivity []bool
	inactivePattern := "^Network.*" + serverAddress + ".*Z$"
	activePattern := "^Network.*" + serverAddress + ".*nil Timestamp.$"
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
	s.cleanupContainers("container-stats")
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
			strings.TrimPrefix(name, containerNamePrefix),
			stat.Mean(cpu, nil), stat.StdDev(cpu, nil))
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
