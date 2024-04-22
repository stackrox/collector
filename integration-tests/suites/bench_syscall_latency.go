package suites

import (
	"encoding/json"
	"fmt"
	"strconv"
	"strings"
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/common"
	"github.com/stackrox/collector/integration-tests/pkg/config"

	"github.com/montanaflynn/stats"
)

const latencyWaitTime = 10 * time.Second

var (
	bccCommandBenchmark = config.Benchmarks{
		BccCommand: "/tools/bin/syscall-latency",
	}
)

type SyscallAnalysis struct {
	P90    float64 `json:"p90"`
	Median float64 `json:"median"`
	Mean   float64 `json:"mean"`
	N      int     `json:"n"`
}

type Analysis struct {
	Read  SyscallAnalysis `json:"read"`
	Close SyscallAnalysis `json:"close"`
}

type RawResults struct {
	Read  []uint64 `json:"read"`
	Close []uint64 `json:"close"`
}

type LatencyResults struct {
	Raw      RawResults `json:"raw"`
	Analysis Analysis   `json:"analysis"`
}

type SyscallLatencyBaselineTestSuite struct {
	BenchmarkTestSuiteBase
}

type SyscallLatencyBenchmarkTestSuite struct {
	BenchmarkTestSuiteBase
}

func (s *SyscallLatencyBenchmarkTestSuite) SetupSuite() {
	s.RegisterCleanup("bcc")
	s.StartContainerStats()

	s.StartCollector(false, nil)

	s.StartPerfTools(&bccCommandBenchmark)
}

func (s *SyscallLatencyBenchmarkTestSuite) TestSyscallLatencyBenchmark() {
	fmt.Println("sleeping", latencyWaitTime)
	time.Sleep(latencyWaitTime)
	fmt.Println("slept", latencyWaitTime)
}

func (s *SyscallLatencyBenchmarkTestSuite) TearDownSuite() {
	logs, err := s.containerLogs("bcc")
	s.Require().NoError(err)

	// not using s.StopPerfTools because we don't need these logs
	// - we're going to process them here
	s.cleanupContainers(s.perfContainers...)

	s.StopCollector()
	s.cleanupContainers("bcc")

	analysis, err := processSyscallLatency(strings.Split(logs, "\n"))
	s.Require().NoError(err)

	output, err := common.PrepareLog(s.T().Name(), "results.json")
	defer output.Close()

	encoder := json.NewEncoder(output)
	s.Require().NoError(encoder.Encode(analysis))

	s.WritePerfResults()

}

func (s *SyscallLatencyBaselineTestSuite) SetupSuite() {
	s.RegisterCleanup("bcc")
	s.StartContainerStats()
	s.StartPerfTools(&bccCommandBenchmark)
}

func (s *SyscallLatencyBaselineTestSuite) TestSyscallLatencyBaseline() {
	time.Sleep(latencyWaitTime)
}

func (s *SyscallLatencyBaselineTestSuite) TearDownSuite() {
	logs, err := s.containerLogs("bcc")
	s.Require().NoError(err)

	// not using s.StopPerfTools because we don't need these logs
	// - we're going to process them here
	s.cleanupContainers(s.perfContainers...)

	analysis, err := processSyscallLatency(strings.Split(logs, "\n"))
	s.Require().NoError(err)

	output, err := common.PrepareLog(s.T().Name(), "results.json")
	defer output.Close()

	encoder := json.NewEncoder(output)
	s.Require().NoError(encoder.Encode(analysis))

	s.WritePerfResults()
}

func processSyscallLatency(lines []string) (*LatencyResults, error) {
	raw_read := make([]uint64, 0)
	raw_close := make([]uint64, 0)

	fmt.Println(len(lines))

	for _, line := range lines {
		if line == "" {
			continue
		}

		call := line[0]
		value, err := strconv.Atoi(line[2:])
		if err != nil {
			// if integer conversion fails, simply ignore
			// the line
			continue
		}

		if call == 'r' {
			raw_read = append(raw_read, uint64(value))
		} else if call == 'c' {
			raw_close = append(raw_close, uint64(value))
		}
		// ignore unknown lines
	}

	read_analysis, err := processRawSyscall(raw_read)
	if err != nil {
		return nil, err
	}

	close_analysis, err := processRawSyscall(raw_close)
	if err != nil {
		return nil, err
	}

	return &LatencyResults{
		Raw: RawResults{
			Read:  raw_read,
			Close: raw_close,
		},
		Analysis: Analysis{
			Read:  *read_analysis,
			Close: *close_analysis,
		},
	}, nil
}

func processRawSyscall(raw []uint64) (*SyscallAnalysis, error) {
	data := stats.LoadRawData(raw)

	p90, err := stats.Percentile(data, 90)
	if err != nil {
		return nil, err
	}

	median, err := stats.Median(data)
	if err != nil {
		return nil, err
	}

	mean, err := stats.Mean(data)
	if err != nil {
		return nil, err
	}

	return &SyscallAnalysis{
		P90:    p90,
		Median: median,
		Mean:   mean,
		N:      len(raw),
	}, nil
}
