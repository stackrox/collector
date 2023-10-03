package mock_sensor

import (
	"testing"

	"time"

	"github.com/stretchr/testify/assert"
	"github.com/thoas/go-funk"

	"github.com/stackrox/collector/integration-tests/suites/types"
)

func (s *MockSensor) ExpectProcessesN(t *testing.T, containerID string, timeout time.Duration, n int) []types.ProcessInfo {
	return s.waitProcessesN(func() {
		assert.FailNowf(t, "timed out", "found %d processes (expected %d)", len(s.Processes(containerID)), n)
	}, containerID, timeout, n)
}

func (s *MockSensor) WaitProcessesN(containerID string, timeout time.Duration, n int) bool {
	return len(s.waitProcessesN(func() {}, containerID, timeout, n)) == n
}

func (s *MockSensor) ExpectProcesses(
	t *testing.T, containerID string, timeout time.Duration, expected ...types.ProcessInfo) bool {

	// might have already seen some of the events
	to_find := funk.Filter(expected, func(x types.ProcessInfo) bool {
		return !s.HasProcess(containerID, x)
	}).([]types.ProcessInfo)

	if len(to_find) == 0 {
		// seen them all
		return true
	}

	timer := time.After(timeout)

loop:
	for {
		select {
		case <-timer:
			return assert.ElementsMatch(t, expected, s.Processes(containerID), "Not all processes received")
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
}

func (s *MockSensor) ExpectLineages(t *testing.T, containerID string, timeout time.Duration, processName string, expected ...types.ProcessLineage) bool {
	to_find := funk.Filter(expected, func(x types.ProcessLineage) bool {
		return s.HasLineage(containerID, x)
	}).([]types.ProcessLineage)

	if len(to_find) == 0 {
		return true
	}

	timer := time.After(timeout)

	for {
		select {
		case <-timer:
			return assert.ElementsMatch(t, expected, s.ProcessLineages(containerID), "Not all process lineages received")
		case lineage := <-s.LiveLineages():
			info := types.ProcessLineage{
				Name:          processName,
				ParentExePath: lineage.GetParentExecFilePath(),
				ParentUid:     int(lineage.GetParentUid()),
			}

			to_find = funk.Filter(to_find, func(x types.ProcessLineage) bool {
				return s.HasLineage(containerID, info)
			}).([]types.ProcessLineage)

			if len(to_find) == 0 {
				return true
			}
		}
	}

}

func (s *MockSensor) waitProcessesN(timeoutFn func(), containerID string, timeout time.Duration, n int) []types.ProcessInfo {
	if len(s.Processes(containerID)) == n {
		return s.Processes(containerID)
	}

loop:
	for {
		select {
		case <-time.After(timeout):
			timeoutFn()
			return make([]types.ProcessInfo, 0)
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
