package suites

import (
	"fmt"
	"os"
	"time"

	"github.com/google/shlex"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/stackrox/collector/integration-tests/suites/common"
	"github.com/stackrox/collector/integration-tests/suites/config"
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

func (b *BenchmarkTestSuiteBase) StartPerfTools() {
	benchmark_options := config.BenchmarksInfo()
	perf := benchmark_options.PerfCommand
	bpftrace := benchmark_options.BpftraceCommand
	bcc := benchmark_options.BccCommand

	skipInit := benchmark_options.SkipInit

	if skipInit && (perf == "" && bpftrace == "" && bcc == "") {
		fmt.Fprintf(os.Stderr, "COLLECTOR_SKIP_HEADERS_INIT set, but no performance tool requested - ignoring.")
		return
	}

	if !skipInit && (perf != "" || bpftrace != "" || bcc != "") {
		b.RunInitContainer()
	}

	image_store := config.Images()

	if perf != "" {
		perf_image := image_store.QaImageByKey("performance-perf")
		b.StartPerfContainer("perf", perf_image, perf)
	}

	if bpftrace != "" {
		bpftrace_image := image_store.QaImageByKey("performance-bpftrace")
		b.StartPerfContainer("bpftrace", bpftrace_image, bpftrace)
	}

	if bcc != "" {
		bcc_image := image_store.QaImageByKey("performance-bcc")
		b.StartPerfContainer("bcc", bcc_image, bcc)
	}
}

func (b *BenchmarkTestSuiteBase) StartPerfContainer(name string, image string, args string) {
	argsList, err := shlex.Split(args)
	require.NoError(b.T(), err)
	b.startContainer(name, image, common.QuoteArgs(argsList)...)
}

func (b *BenchmarkTestSuiteBase) RunInitContainer() {
	init_image := config.Images().QaImageByKey("performance-init")
	cmd := []string{
		"-v", "/lib/modules:/lib/modules",
		"-v", "/etc/os-release:/etc/os-release",
		"-v", "/etc/lsb-release:/etc/lsb-release",
		"-v", "/usr/src:/usr/src",
		"-v", "/boot:/boot",
		init_image,
	}

	containerID, err := b.launchContainer("host-init", cmd...)
	require.NoError(b.T(), err)

	if finished, _ := b.waitForContainerToExit("host-init", containerID, 5*time.Second, 0); !finished {
		logs, err := b.containerLogs("host-init")
		if err == nil {
			fmt.Println(logs)
		}
		assert.FailNow(b.T(), "Failed to initialize host for performance testing")
	}
	b.cleanupContainers(containerID)
}

func (b *BenchmarkTestSuiteBase) startContainer(name string, image string, args ...string) {
	cmd := []string{
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

	containerID, err := b.launchContainer(name, cmd...)
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
	s.RegisterCleanup("perf", "bcc", "bpftrace", "init")
	s.StartContainerStats()

	s.StartPerfTools()

	s.StartCollector(false, nil)
}

func (s *BenchmarkCollectorTestSuite) TestBenchmarkCollector() {
	s.RunCollectorBenchmark()
}

func (s *BenchmarkCollectorTestSuite) TearDownSuite() {
	s.StopPerfTools()

	s.StopCollector()

	s.cleanupContainers("benchmark")
	s.WritePerfResults()
}

func (s *BenchmarkBaselineTestSuite) SetupSuite() {
	s.StartContainerStats()
	s.StartPerfTools()
}

func (s *BenchmarkBaselineTestSuite) TestBenchmarkBaseline() {
	s.RunCollectorBenchmark()
}

func (s *BenchmarkBaselineTestSuite) TearDownSuite() {
	s.StopPerfTools()
	s.cleanupContainers("benchmark")
	s.WritePerfResults()
}
