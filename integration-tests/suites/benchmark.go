package suites

import (
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/google/shlex"
	"github.com/stackrox/collector/integration-tests/pkg/log"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/stackrox/collector/integration-tests/pkg/collector"
	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"
)

type BenchmarkBaselineTestSuite struct {
	BenchmarkTestSuiteBase
}

type BenchmarkCollectorTestSuite struct {
	BenchmarkTestSuiteBase
	workloads []string
}

type BenchmarkTestSuiteBase struct {
	IntegrationTestSuiteBase
	perfContainers     []string
	loadContainers     []string
	profileContainerID string
}

func (b *BenchmarkTestSuiteBase) StartPerfTools() {
	benchmark_options := config.BenchmarksInfo()
	perf := benchmark_options.PerfCommand
	bpftrace := benchmark_options.BpftraceCommand
	bcc := benchmark_options.BccCommand

	skipInit := benchmark_options.SkipInit

	if skipInit && (perf == "" && bpftrace == "" && bcc == "") {
		fmt.Fprintf(os.Stderr, "COLLECTOR_SKIP_HEADERS_INIT set, but no performance tool requested - ignoring.")
		log.Warn("COLLECTOR_SKIP_HEADERS_INIT set, but no performance tool requested - ignoring.")
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

func (b *BenchmarkTestSuiteBase) StartCPUProfile() {
	benchmarkOptions := config.BenchmarksInfo()
	if !benchmarkOptions.CPUProfile {
		return
	}

	resultDir, err := filepath.Abs(filepath.Join(config.LogPath(), strings.SplitN(b.T().Name(), "/", 2)[0]))
	b.Require().NoError(err)
	b.Require().NoError(os.MkdirAll(resultDir, os.ModePerm))
	b.Require().NoError(b.Executor().CopyFileFromContainer(
		b.Collector().ContainerID(),
		"/usr/local/bin/collector",
		filepath.Join(resultDir, "rootfs/usr/local/bin/collector")))
	collectorPID, err := b.Executor().GetContainerPID(b.Collector().ContainerID())
	b.Require().NoError(err)
	containerID, err := b.Executor().StartContainer(config.ContainerStartConfig{
		Name:       "cpu-profile",
		Image:      config.Images().QaImageByKey("performance-perf"),
		Privileged: true,
		PidMode:    "host",
		Mounts:     map[string]string{"/results": resultDir},
		Env: map[string]string{
			"PERF_FREQUENCY": benchmarkOptions.CPUProfileFreq,
			"PERF_OUTPUT_FILE": "/results/perf.data",
		},
		Command: []string{"record", "--buildid-all", "-e", "cpu-clock", "-F", benchmarkOptions.CPUProfileFreq, "-g", "--call-graph", "dwarf", "-p", strconv.Itoa(collectorPID), "-o", "/results/perf.data", "--", "sleep", "70"},
	})
	b.Require().NoError(err)
	b.profileContainerID = containerID
	b.perfContainers = append(b.perfContainers, containerID)

}

func (b *BenchmarkTestSuiteBase) StartCPUProfileCapture() {
	b.StartCPUProfile()
}

func (b *BenchmarkTestSuiteBase) StopCPUProfileCapture() {
	if b.profileContainerID == "" {
		return
	}
	finished, err := b.waitForContainerToExit("cpu-profile", b.profileContainerID, 100*time.Millisecond, 5*time.Minute)
	b.Require().NoError(err)
	b.Require().True(finished, "CPU profiler did not finish")
}

func (b *BenchmarkTestSuiteBase) StartPerfContainer(name string, image string, args string) {
	argsList, err := shlex.Split(args)
	require.NoError(b.T(), err)
	b.startContainer(name, image, common.QuoteArgs(argsList)...)
}

func (b *BenchmarkTestSuiteBase) RunInitContainer() {
	containerID, err := b.Executor().StartContainer(
		config.ContainerStartConfig{
			Name:  "host-init",
			Image: config.Images().QaImageByKey("performance-init"),
			Mounts: map[string]string{
				"/lib/modules":     "/lib/modules",
				"/etc/os-release":  "/etc/os-release",
				"/etc/lsb-release": "/etc/lsb-release",
				"/usr/src":         "/usr/src",
				"/boot":            "/boot",
			},
		})
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
	containerID, err := b.Executor().StartContainer(
		config.ContainerStartConfig{
			Name:       name,
			Image:      image,
			Privileged: true,
			Mounts: map[string]string{
				"/sys":         "/sys",
				"/usr/src":     "/usr/src",
				"/lib/modules": "/lib/modules",
				"/tmp":         "/tmp",
			},
		})
	require.NoError(b.T(), err)

	b.perfContainers = append(b.perfContainers, containerID)
}

func (b *BenchmarkTestSuiteBase) FetchWorkloadLogs() {
	for _, container := range b.loadContainers {
		containerLog, err := b.containerLogs(container)
		require.NoError(b.T(), err)
		log.Info("benchmark workload log: %s %s`", container, containerLog)
	}

	b.loadContainers = nil
}

func (b *BenchmarkTestSuiteBase) StopPerfTools() {
	b.stopContainers(b.perfContainers...)

	for _, container := range b.perfContainers {
		log, err := b.containerLogs(container)
		require.NoError(b.T(), err)

		fmt.Println(log)
		if container == b.profileContainerID {
			_, err = b.Executor().CaptureLogs(strings.SplitN(b.T().Name(), "/", 2)[0], "cpu-profile")
			require.NoError(b.T(), err)
		}
	}

	b.removeContainers(b.perfContainers...)
	b.perfContainers = nil
}

func (s *BenchmarkCollectorTestSuite) SetupSuite() {
	s.RegisterCleanup("perf", "cpu-profile", "bcc", "bpftrace", "init",
		"benchmark-processes", "benchmark-endpoints", "benchmark-connections")

	s.StartPerfTools()

	var collectorOptions *collector.StartupOptions
	if config.BenchmarksInfo().EnableScrape {
		collectorOptions = &collector.StartupOptions{
			Config: map[string]any{
				"turnOffScrape":  false,
				"scrapeInterval": 1,
			},
			Env: map[string]string{
				"ROX_PROCESSES_LISTENING_ON_PORT": "true",
			},
		}
	}
	s.StartCollector(false, collectorOptions)
}

func (s *BenchmarkTestSuiteBase) SpinBerserker(workload string) (string, error) {
	benchmarkImage := config.Images().QaImageByKey("performance-berserker")
	err := s.Executor().PullImage(benchmarkImage)
	if err != nil {
		return "", err
	}

	containerID, err := s.Executor().StartContainer(
		config.ContainerStartConfig{
			Name:    fmt.Sprintf("benchmark-%s", workload),
			Image:   benchmarkImage,
			Command: []string{fmt.Sprintf("/etc/berserker/%s/workload.toml", workload)},
		})
	if err != nil {
		return "", err
	}

	s.loadContainers = append(s.loadContainers, containerID)
	return containerID, nil
}

func (s *BenchmarkTestSuiteBase) SpinNetworkBerserker() (string, error) {
	benchmarkImage := config.Images().QaImageByKey("performance-berserker")
	if err := s.Executor().PullImage(benchmarkImage); err != nil {
		return "", err
	}

	containerID, err := s.Executor().StartContainer(config.ContainerStartConfig{
		Name:       "benchmark-connections",
		Image:      benchmarkImage,
		Privileged: true,
		Entrypoint: []string{"/scripts/init.sh"},
		Env: map[string]string{
			"BERSERKER__DURATION": "60",
			"IP_BASE":             "223.42.0.1/16",
		},
	})
	if err != nil {
		return "", err
	}
	s.loadContainers = append(s.loadContainers, containerID)
	return containerID, nil
}

func (s *BenchmarkTestSuiteBase) RunCollectorBenchmark() {
	s.start = time.Now().UTC()
	s.StartCPUProfileCapture()

	benchmarkContainers := make([]string, 0, len(config.BenchmarksInfo().Workloads))
	var networkContainerID string
	for _, workload := range config.BenchmarksInfo().Workloads {
		if workload == "connections" {
			containerID, err := s.SpinNetworkBerserker()
			s.Require().NoError(err)
			benchmarkContainers = append(benchmarkContainers, containerID)
			networkContainerID = common.ContainerShortID(containerID)
			continue
		}
		containerID, err := s.SpinBerserker(workload)
		s.Require().NoError(err)
		benchmarkContainers = append(benchmarkContainers, containerID)
	}

	// The assumption is that the benchmark is short, and to get better
	// resolution into when relevant metrics start and stop, tick more
	// frequently
	waitTick := 1 * time.Second

	// Container name here is used only for reporting
	for _, containerID := range benchmarkContainers {
		_, err := s.waitForContainerToExit("berserker", containerID, waitTick, 0)
		s.Require().NoError(err)
	}
	if networkContainerID != "" {
		s.Require().NotEmpty(s.Sensor().Connections(networkContainerID), "network workload produced no Collector connection signals")
	}

	s.stop = time.Now().UTC()
	s.StopCPUProfileCapture()
}

func (s *BenchmarkCollectorTestSuite) TestBenchmarkCollector() {
	s.RunCollectorBenchmark()
}

func (s *BenchmarkCollectorTestSuite) TearDownSuite() {
	s.StopPerfTools()
	s.FetchWorkloadLogs()

	s.StopCollector()

	s.cleanupContainers("benchmark")
	s.WritePerfResults()
}

func (s *BenchmarkBaselineTestSuite) SetupSuite() {
	s.RegisterCleanup("benchmark-processes", "benchmark-endpoints")
	s.StartPerfTools()
}

func (s *BenchmarkBaselineTestSuite) TestBenchmarkBaseline() {
	s.RunCollectorBenchmark()
}

func (s *BenchmarkBaselineTestSuite) TearDownSuite() {
	s.StopPerfTools()
	s.FetchWorkloadLogs()
	s.cleanupContainers("benchmark")
	s.WritePerfResults()
}
