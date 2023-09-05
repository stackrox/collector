package common

import (
	"testing"

	"time"

	"github.com/stretchr/testify/assert"
	"github.com/thoas/go-funk"

	"github.com/stackrox/collector/integration-tests/suites/types"
)

func (c *CollectorManager) ExpectProcesses(
	t *testing.T, containerID string, timeout time.Duration, expected ...types.ProcessInfo) bool {

	// might have already seen some of the events
	to_find := funk.Filter(expected, func(x types.ProcessInfo) bool {
		return c.Sensor.HasProcess(containerID, x)
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

		case process := <-c.Sensor.LiveProcesses():
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

			if len(expected) == 0 {
				return true
			}
		}
	}

	return assert.ElementsMatch(t, expected, c.Sensor.Processes(containerID), "Not all processes received")
}

func (c *CollectorManager) ExpectNetworks(t *testing.T, containerID string, timeout time.Duration, expected ...types.NetworkInfo) bool {

	to_find := funk.Filter(expected, func(x types.NetworkInfo) bool {
		return c.Sensor.HasConnection(containerID, x)
	}).([]types.NetworkInfo)

	if len(to_find) == 0 {
		return true
	}

loop:
	for {
		select {
		case <-time.After(timeout):
			return assert.Fail(t, "timed out waiting for networks")
		case network := <-c.Sensor.LiveConnections():
			if network.GetContainerId() != containerID {
				continue loop
			}

			to_find = funk.Filter(expected, func(x types.NetworkInfo) bool {
				return c.Sensor.HasConnection(containerID, x)
			}).([]types.NetworkInfo)

			if len(to_find) == 0 {
				return true
			}
		}
	}

	// technically we know they don't match at this point, but by using
	// ElementsMatch we get much better logging about the differences
	return assert.ElementsMatch(t, expected, c.Sensor.Connections(containerID))
}
