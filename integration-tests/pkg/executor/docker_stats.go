package executor

import (
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/docker/docker/api/types"
	"github.com/pkg/errors"
	"github.com/stackrox/collector/integration-tests/pkg/log"
)

// ContainerStat represents the CPU and memory statistics of a container at a specific time.
type ContainerStat struct {
	Timestamp string  `json:"Timestamp"`
	Id        string  `json:"Id"`
	Name      string  `json:"Name"`
	Mem       string  `json:"Mem"`
	Cpu       float64 `json:"Cpu"`
}

// StatsCollector handles the collection of container stats.
type ContainerRuntimeStatsPoller struct {
	executor   Executor
	containers []string
	stats      []ContainerStat
	mu         sync.Mutex
	running    bool
	stopChan   chan struct{}
	wg         sync.WaitGroup
}

// NewContainerRuntimeStatsPoller initializes a new StatsCollector.
func NewContainerRuntimeStatsPoller(executor Executor, containerIds []string) (*ContainerRuntimeStatsPoller, error) {
	return &ContainerRuntimeStatsPoller{
		executor:   executor,
		containers: containerIds,
		stopChan:   make(chan struct{}),
	}, nil
}

// Start begins collecting stats for the specified containers.
func (s *ContainerRuntimeStatsPoller) Start() {
	if s.running {
		return
	}

	s.running = true

	for _, containerID := range s.containers {
		s.wg.Add(1)
		go s.collectStats(containerID)
	}
	log.Info("gathering cpu/mem stats for containers: %v", s.containers)
}

// Stop stops the stats collection and waits for all collection goroutines to finish.
func (s *ContainerRuntimeStatsPoller) Stop() {
	if !s.running {
		log.Trace("stats collector is already stopped")
		return
	}

	close(s.stopChan)
	s.wg.Wait()
	s.running = false
	log.Info("stopped gathering cpu/mem stats for containers %v", s.containers)
}

// GetStats retrieves the collected stats after the collection has stopped.
func (s *ContainerRuntimeStatsPoller) GetStats() []ContainerStat {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.stats
}

func (s *ContainerRuntimeStatsPoller) collectStats(containerID string) {
	defer s.wg.Done()

	for {
		time.Sleep(1 * time.Second)
		select {
		case <-s.stopChan:
			return
		default:
			containerStat, err := s.executor.GetContainerStat(containerID)
			if err != nil {
				continue
			}
			s.mu.Lock()
			s.stats = append(s.stats, *containerStat)
			s.mu.Unlock()

		}
	}
}

func ToContainerStat(statsJSON *types.StatsJSON) (*ContainerStat, error) {
	if statsJSON == nil || statsJSON.Name == "" {
		return nil, errors.New("empty input")
	}
	return &ContainerStat{
		Timestamp: time.Now().Format("2006-01-02 15:04:05"),
		Id:        statsJSON.ID[:min(12, len(statsJSON.ID))],
		Name:      strings.TrimPrefix(statsJSON.Name, "/"),
		Mem:       fmt.Sprintf("%.2fMiB", float64(statsJSON.MemoryStats.Usage)/(1024*1024)),
		Cpu:       calculateCPUPercent(statsJSON),
	}, nil
}

// cpuPercent = (cpuDelta / systemDelta) * onlineCPUs * 100.0
// https://stackoverflow.com/questions/47401648/docker-stats-with-cpu-percentage-more-than-100
func calculateCPUPercent(stats *types.StatsJSON) float64 {
	cpuDelta := float64(stats.CPUStats.CPUUsage.TotalUsage - stats.PreCPUStats.CPUUsage.TotalUsage)
	systemDelta := float64(stats.CPUStats.SystemUsage - stats.PreCPUStats.SystemUsage)

	if systemDelta > 0.0 && cpuDelta > 0.0 {
		return (cpuDelta / systemDelta) * float64(stats.CPUStats.OnlineCPUs) * 100.0
	}
	return 0.0
}
