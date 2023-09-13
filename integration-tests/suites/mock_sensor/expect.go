package mock_sensor

import (
	"testing"

	"time"

	"github.com/stretchr/testify/assert"
	"github.com/thoas/go-funk"

	"github.com/stackrox/collector/integration-tests/suites/types"
)

func (s *MockSensor) ExpectProcesses(
	t *testing.T, containerID string, timeout time.Duration, expected ...types.ProcessInfo) bool {

	// might have already seen some of the events
	to_find := funk.Filter(expected, func(x types.ProcessInfo) bool {
		return s.HasProcess(containerID, x)
	}).([]types.ProcessInfo)

	if len(to_find) == 0 {
		// seen them all
		return true
	}

loop:
	for {
		select {
		case <-time.After(timeout):
			break loop

		case process := <-s.LiveProcesses():
			if process.GetContainerId() != containerID {
				continue loop
			}

			info := types.ProcessInfo{
				Name:    process.GetName(),
				ExePath: process.GetExecFilePath(),
				// Pid:     int(process.GetPid()),
				Uid:  int(process.GetUid()),
				Gid:  int(process.GetGid()),
				Args: process.GetArgs(),
			}

			to_find = funk.Filter(to_find, func(x types.ProcessInfo) bool {
				return x != info
			}).([]types.ProcessInfo)

			if len(to_find) == 0 {
				return true
			}
		}
	}

	return assert.ElementsMatch(t, expected, s.Processes(containerID), "Not all processes received")
}

func (s *MockSensor) ExpectLineages(t *testing.T, containerID string, timeout time.Duration, processName string, expected ...types.ProcessLineage) bool {
	to_find := funk.Filter(expected, func(x types.ProcessLineage) bool {
		return s.HasLineage(containerID, x)
	}).([]types.ProcessLineage)

	if len(to_find) == 0 {
		return true
	}

loop:
	for {
		select {
		case <-time.After(timeout):
			break loop
		case lineage := <-s.LiveLineages():
			info := types.ProcessLineage{
				Name:          processName,
				ParentExePath: lineage.GetParentExecFilePath(),
				ParentUid:     int(lineage.GetParentUid()),
			}

			to_find = funk.Filter(to_find, func(x types.ProcessLineage) bool {
				return x != info
			}).([]types.ProcessLineage)

			if len(to_find) == 0 {
				return true
			}
		}
	}

	return assert.ElementsMatch(t, expected, s.ProcessLineages(containerID), "Not all process lineages received")
}

func (s *MockSensor) ExpectConnections(t *testing.T, containerID string, timeout time.Duration, expected ...types.NetworkInfo) bool {

	to_find := funk.Filter(expected, func(x types.NetworkInfo) bool {
		return s.HasConnection(containerID, x)
	}).([]types.NetworkInfo)

	if len(to_find) == 0 {
		return true
	}

loop:
	for {
		select {
		case <-time.After(timeout):
			return assert.Fail(t, "timed out waiting for networks")
		case network := <-s.LiveConnections():
			if network.GetContainerId() != containerID {
				continue loop
			}

			to_find = funk.Filter(expected, func(x types.NetworkInfo) bool {
				return s.HasConnection(containerID, x)
			}).([]types.NetworkInfo)

			if len(to_find) == 0 {
				return true
			}
		}
	}

	// technically we know they don't match at this point, but by using
	// ElementsMatch we get much better logging about the differences
	return assert.ElementsMatch(t, expected, s.Connections(containerID))
}

func (s *MockSensor) ExpectNConnections(t *testing.T, containerID string, timeout time.Duration, n int) []types.NetworkInfo {
	if len(s.Connections(containerID)) == n {
		return s.Connections(containerID)
	}

loop:
	for {
		select {
		case <-time.After(timeout):
			assert.FailNowf(t, "timed out", "Only found %d/%d connections", len(s.Connections(containerID)), n)
		case conn := <-s.LiveConnections():
			if conn.GetContainerId() != containerID {
				continue loop
			}

			if len(s.Connections(containerID)) == n {
				return s.Connections(containerID)
			}
		}
	}
}

func (s *MockSensor) ExpectNEndpoints(t *testing.T, containerID string, timeout time.Duration, n int) []types.EndpointInfo {
	if len(s.Endpoints(containerID)) == n {
		return s.Endpoints(containerID)
	}

loop:
	for {
		select {
		case <-time.After(timeout):
			assert.FailNowf(t, "timed out", "Only found %d/%d connections", len(s.Endpoints(containerID)), n)
		case ep := <-s.LiveEndpoints():
			if ep.GetContainerId() != containerID {
				continue loop
			}

			if len(s.Endpoints(containerID)) == n {
				return s.Endpoints(containerID)
			}
		}
	}
}

func (s *MockSensor) ExpectNProcesses(t *testing.T, containerID string, timeout time.Duration, n int) []types.ProcessInfo {
	if len(s.Processes(containerID)) == n {
		return s.Processes(containerID)
	}

loop:
	for {
		select {
		case <-time.After(timeout):
			assert.FailNowf(t, "timed out", "Only found %d/%d processes", len(s.Processes(containerID)), n)
		case proc := <-s.LiveProcesses():
			if proc.GetContainerId() != containerID {
				continue loop
			}

			if len(s.Processes(containerID)) == n {
				return s.Processes(containerID)
			}
		}
	}
}
