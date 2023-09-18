package mock_sensor

import (
	"testing"

	"time"

	"github.com/stretchr/testify/assert"
	"github.com/thoas/go-funk"

	"github.com/stackrox/collector/integration-tests/suites/types"
)

// ExpectConnections waits up to the timeout for the gRPC server to receive
// the list of expected Connections. It will first check to see if the connections
// have been received already, and then monitor the live feed of connections
// until timeout or until all the events have been received.
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

// ExpectConnectionsN waits up to the timeout for the gRPC server to receive
// the a set number of connections. It will first check to see if the connections
// have been received already, and then monitor the live feed of connections
// until timeout or until all the events have been received.
//
// It does not consider the content of the events, just that a certain number
// have been received
func (s *MockSensor) ExpectConnectionsN(t *testing.T, containerID string, timeout time.Duration, n int) []types.NetworkInfo {
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

// ExpectEndpointsN waits up to the timeout for the gRPC server to receive
// the a set number of endpoints. It will first check to see if the endpoints
// have been received already, and then monitor the live feed of endpoints
// until timeout or until all the events have been received.
//
// It does not consider the content of the events, just that a certain number
// have been received
func (s *MockSensor) ExpectEndpointsN(t *testing.T, containerID string, timeout time.Duration, n int) []types.EndpointInfo {
	return s.waitEndpointsN(func() {
		assert.FailNowf(t, "timed out", "Only found %d/%d connections", len(s.Endpoints(containerID)), n)
	}, containerID, timeout, n)
}

// WaitEndpointsN is a non-fatal version of ExpectEndpointsN. It waits for a given timeout
// until n Endpoints have been receieved. On timeout it returns false.
func (s *MockSensor) WaitEndpointsN(containerID string, timeout time.Duration, n int) bool {
	return len(s.waitEndpointsN(func() {}, containerID, timeout, n)) == n
}

// waitEndpointsN is a helper function for waiting for a set number of endpoints.
// the timeoutFn function can be used to control error behaviour on timeout.
func (s *MockSensor) waitEndpointsN(timeoutFn func(), containerID string, timeout time.Duration, n int) []types.EndpointInfo {
	if len(s.Endpoints(containerID)) == n {
		return s.Endpoints(containerID)
	}

loop:
	for {
		select {
		case <-time.After(timeout):
			timeoutFn()
			return make([]types.EndpointInfo, 0)
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
