package suites

import (
	"fmt"
	"os"
	"strconv"
	"time"

	"github.com/stackrox/collector/integration-tests/pkg/config"
	"github.com/stackrox/collector/integration-tests/pkg/executor"
)

const (
	densityServersPerGroup = 3
	densityClientsPerGroup = 2
)

type densityGroup struct {
	containers []string
}

func densitySetting(name string, defaultValue int) (int, error) {
	value := os.Getenv(name)
	if value == "" {
		return defaultValue, nil
	}
	n, err := strconv.Atoi(value)
	if err != nil || n <= 0 {
		return 0, fmt.Errorf("%s must be a positive integer: %q", name, value)
	}
	return n, nil
}

func (s *BenchmarkTestSuiteBase) RunDensityBenchmark() {
	groups, err := densitySetting("COLLECTOR_BENCHMARK_DENSITY_GROUPS", 10)
	s.Require().NoError(err)
	steadySeconds, err := densitySetting("COLLECTOR_BENCHMARK_DENSITY_STEADY_SECONDS", 60)
	s.Require().NoError(err)
	intervalSeconds, err := densitySetting("COLLECTOR_BENCHMARK_DENSITY_CHURN_INTERVAL_SECONDS", 30)
	s.Require().NoError(err)
	cycles, err := densitySetting("COLLECTOR_BENCHMARK_DENSITY_CHURN_CYCLES", 6)
	s.Require().NoError(err)

	images := config.Images()
	serverImage := images.QaImageByKey("qa-nginx")
	clientImage := images.QaImageByKey("qa-alpine-curl")
	s.Require().NoError(s.Executor().PullImage(serverImage))
	s.Require().NoError(s.Executor().PullImage(clientImage))

	active := make(map[string]struct{})
	defer func() {
		for id := range active {
			_, _ = s.Executor().RemoveContainer(executor.ContainerFilter{Name: id})
		}
	}()

	startGroup := func(group, generation int) densityGroup {
		var result densityGroup
		var serverIP string
		for i := 0; i < densityServersPerGroup; i++ {
			id, err := s.Executor().StartContainer(config.ContainerStartConfig{
				Name:  fmt.Sprintf("density-%d-%d-server-%d", group, generation, i),
				Image: serverImage,
			})
			s.Require().NoError(err)
			active[id] = struct{}{}
			result.containers = append(result.containers, id)
			if i == 0 {
				serverIP, err = s.Executor().GetContainerIP(id)
				s.Require().NoError(err)
				s.Require().NotEmpty(serverIP)
			}
		}
		for i := 0; i < densityClientsPerGroup; i++ {
			id, err := s.Executor().StartContainer(config.ContainerStartConfig{
				Name:    fmt.Sprintf("density-%d-%d-client-%d", group, generation, i),
				Image:   clientImage,
				Command: []string{"sh", "-c", fmt.Sprintf("while true; do curl --silent --fail --max-time 2 http://%s/ -o /dev/null; sleep 10; done", serverIP)},
			})
			s.Require().NoError(err)
			active[id] = struct{}{}
			result.containers = append(result.containers, id)
		}
		return result
	}

	stopGroup := func(group densityGroup) {
		// Clients stop sending requests before their server disappears.
		for i := len(group.containers) - 1; i >= 0; i-- {
			id := group.containers[i]
			_, err := s.Executor().StopContainer(id)
			s.Require().NoError(err)
			_, err = s.Executor().RemoveContainer(executor.ContainerFilter{Name: id})
			s.Require().NoError(err)
			delete(active, id)
		}
	}

	population := make([]densityGroup, groups)
	for i := range population {
		population[i] = startGroup(i, 0)
	}
	s.AddMetric("density_containers", float64(groups*(densityServersPerGroup+densityClientsPerGroup)))
	s.AddMetric("density_churn_interval_seconds", float64(intervalSeconds))

	steadyStart := time.Now().UTC()
	time.Sleep(time.Duration(steadySeconds) * time.Second)
	s.AddBenchmarkPhase("steady", steadyStart, time.Now().UTC())

	// Profile churn rather than image pulls and the initial population creation.
	s.StartCPUProfileCapture()
	churnStart := time.Now().UTC()
	interval := time.Duration(intervalSeconds) * time.Second
	for cycle := 0; cycle < cycles; cycle++ {
		cycleStart := time.Now()
		group := cycle % len(population)
		stopGroup(population[group])
		population[group] = startGroup(group, cycle+1)
		if remaining := interval - time.Since(cycleStart); remaining > 0 {
			time.Sleep(remaining)
		}
	}
	s.AddBenchmarkPhase("churn", churnStart, time.Now().UTC())
	s.AddMetric("density_churn_cycles", float64(cycles))
	s.StopCPUProfileCapture()
}
