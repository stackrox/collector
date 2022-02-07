package integrationtests

import (
	"fmt"
	"testing"
	"time"

	"github.com/google/shlex"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/stretchr/testify/suite"
)

type BenchmarkBaselineTestSuite struct {
	BenchmarkTestSuiteBase
}

type BenchmarkCollectorTestSuite struct {
	BenchmarkTestSuiteBase
}

type BenchmarkTestSuiteBase struct {
	IntegrationTestSuiteBase
	perfContainers []string
}

func TestBenchmarkBaseline(t *testing.T) {
	suite.Run(t, new(BenchmarkBaselineTestSuite))
}

func TestBenchmarkCollector(t *testing.T) {
	suite.Run(t, new(BenchmarkCollectorTestSuite))
}

func (b *BenchmarkTestSuiteBase) StartPerfTools() {
	perf := ReadEnvVar("COLLECTOR_PERF_COMMAND")
	bpftrace := ReadEnvVar("COLLECTOR_BPFTRACE_COMMAND")
	bcc := ReadEnvVar("COLLECTOR_BCC_COMMAND")

	skipInit := ReadBoolEnvVar("COLLECTOR_PERF_SKIP_INIT")

	if !skipInit && (perf != "" || bpftrace != "" || bcc != "") {
		b.RunInitContainer()
	}

	if perf != "" {
		b.StartPerfContainer("perf", "stackrox/collector-performance:perf", perf)
	}

	if bpftrace != "" {
		b.StartPerfContainer("bpftrace", "stackrox/collector-performance:bpftrace", bpftrace)
	}

	if bcc != "" {
		b.StartPerfContainer("bcc", "stackrox/collector-performance:bcc", bcc)
	}
}

func (b *BenchmarkTestSuiteBase) StartPerfContainer(name string, image string, args string) {
	argsList, err := shlex.Split(args)
	require.NoError(b.T(), err)
	b.startContainer(name, image, argsList...)
}

func (b *BenchmarkTestSuiteBase) RunInitContainer() {
	cmd := []string{
		"host-init",
		"-v", "/lib/modules:/host/lib/modules",
		"-v", "/etc/os-release:/host/etc/os-release",
		"-v", "/etc/lsb-release:/host/etc/lsb-release",
		"-v", "/usr/src:/host/usr/src",
		"stackrox/collector-performance:init",
	}

	containerID, err := b.launchContainer(cmd...)
	require.NoError(b.T(), err)

	if finished, _ := b.waitForContainerToExit("host-init", containerID, 5*time.Second); !finished {
		assert.FailNow(b.T(), "Failed to initialize host for performance testing")
	}
	b.cleanupContainer([]string{containerID})
}

func (b *BenchmarkTestSuiteBase) startContainer(name string, image string, args ...string) {
	cmd := []string{
		name,
		"--privileged",
		"-v", "/sys:/sys",
		"-v", "/usr/src:/usr/src",
		"-v", "/lib/modules:/lib/modules",
		// by mounting /tmp we can allow tools to write stats/logs to a local path
		// for later processing
		"-v", "/tmp:/tmp",
		image,
	}

	cmd = append(cmd, args...)

	containerID, err := b.launchContainer(cmd...)
	require.NoError(b.T(), err)

	b.perfContainers = append(b.perfContainers, containerID)
}

func (b *BenchmarkTestSuiteBase) StopPerfTools() {
	b.stopContainers(b.perfContainers...)

	for _, container := range b.perfContainers {
		log, err := b.containerLogs(container)
		require.NoError(b.T(), err)

		fmt.Println(log)
	}

	b.removeContainers(b.perfContainers...)
	b.perfContainers = nil
}

func (s *BenchmarkCollectorTestSuite) SetupSuite() {
	s.executor = NewExecutor()
	s.StartContainerStats()
	s.collector = NewCollectorManager(s.executor, s.T().Name())
	s.metrics = map[string]float64{}

	err := s.collector.Setup()
	require.NoError(s.T(), err)

	s.StartPerfTools()

	err = s.collector.Launch()
	require.NoError(s.T(), err)
}

func (s *BenchmarkCollectorTestSuite) TestBenchmarkCollector() {
	s.RunCollectorBenchmark()
}

func (s *BenchmarkCollectorTestSuite) TearDownSuite() {
	s.StopPerfTools()

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
	s.StartPerfTools()
}

func (s *BenchmarkBaselineTestSuite) TestBenchmarkBaseline() {
	s.RunCollectorBenchmark()
}

func (s *BenchmarkBaselineTestSuite) TearDownSuite() {
	s.StopPerfTools()
	s.cleanupContainer([]string{"benchmark"})
	stats := s.GetContainerStats()
	s.PrintContainerStats(stats)
	s.WritePerfResults("baseline_benchmark", stats, s.metrics)
}
