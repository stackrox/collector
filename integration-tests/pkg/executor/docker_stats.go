package executor

import (
	"context"
	"encoding/json"
	"fmt"
	"sync"
	"time"

	"github.com/docker/docker/api/types"
	"github.com/docker/docker/client"
	"github.com/stackrox/collector/integration-tests/pkg/log"
)

// ContainerStat represents the CPU and memory statistics of a container at a specific time.
type ContainerStat struct {
	Timestamp string  `json:"timestamp"`
	Id        string  `json:"id"`
	Name      string  `json:"name"`
	Mem       string  `json:"mem"`
	Cpu       float64 `json:"cpu"`
}

// StatsCollector handles the collection of container stats.
type ContainerRuntimeStatsPoller struct {
	client     *client.Client
	containers []string
	stats      []ContainerStat
	mu         sync.Mutex
	running    bool
	stopChan   chan struct{}
	wg         sync.WaitGroup
}

// NewStatsCollector initializes a new StatsCollector.
func NewStatsCollector(containerIds []string) (*ContainerRuntimeStatsPoller, error) {
	cli, err := client.NewClientWithOpts(client.FromEnv, client.WithAPIVersionNegotiation())
	if err != nil {
		return nil, err
	}

	return &ContainerRuntimeStatsPoller{
		client:     cli,
		containers: containerIds,
		stopChan:   make(chan struct{}),
	}, nil
}

// Start begins collecting stats for the specified containers.
func (s *ContainerRuntimeStatsPoller) Start() {
	if s.running {
		log.Trace("stats collector is already running")
		return
	}

	s.running = true

	for _, containerID := range s.containers {
		s.wg.Add(1)
		go s.collectStats(containerID)
	}
	log.Info("started stats collector for containers %v", s.containers)
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
	log.Info("stopped stats collector for containers %v", s.containers)
}

// GetStats retrieves the collected stats after the collection has stopped.
func (s *ContainerRuntimeStatsPoller) GetStats() []ContainerStat {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.stats
}

// TODO no error if container doesn't exist, use executor as caller
func getContainerStatJSON(cli *client.Client, containerID string) (types.StatsJSON, error) {
	ctx := context.Background()
	stats, err := cli.ContainerStats(ctx, containerID, false)
	if err != nil {
		return types.StatsJSON{}, err
	}
	defer stats.Body.Close()
	decoder := json.NewDecoder(stats.Body)

	var statsJSON types.StatsJSON
	if err := decoder.Decode(&statsJSON); err != nil {
		return types.StatsJSON{}, err
	}
	return statsJSON, nil
}

func (s *ContainerRuntimeStatsPoller) collectStats(containerID string) {
	defer s.wg.Done()

	for {
		time.Sleep(1 * time.Second)
		select {
		case <-s.stopChan:
			return
		default:
			statsJSON, err := getContainerStatJSON(s.client, containerID)
			if err != nil {
				continue
			}
			containerStat := ContainerStat{
				Timestamp: time.Now().Format(time.RFC3339),
				Id:        statsJSON.ID,
				Name:      containerID,
				Mem:       fmt.Sprintf("%.2fMiB", float64(statsJSON.MemoryStats.Usage)/(1024*1024)),
				Cpu:       calculateCPUPercent(&statsJSON),
			}

			s.mu.Lock()
			s.stats = append(s.stats, containerStat)
			s.mu.Unlock()

		}
	}
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
