package mock_sensor

import (
	"testing"

	"time"

	"github.com/stretchr/testify/assert"
	"github.com/thoas/go-funk"

	"github.com/stackrox/collector/integration-tests/pkg/types"
)

// ExpectConnections waits up to the timeout for the gRPC server to receive
// the list of expected Connections. It will first check to see if the connections
// have been received already, and then monitor the live feed of connections
// until timeout or until all the events have been received.
func (s *MockSensor) ExpectConnections(t *testing.T, containerID string, timeout time.Duration, expected ...types.NetworkInfo) bool {

	to_find := funk.Filter(expected, func(x types.NetworkInfo) bool {
		return !s.HasConnection(containerID, x)
	}).([]types.NetworkInfo)

	if len(to_find) == 0 {
		return true
	}

	timer := time.After(timeout)

loop:
	for {
		select {
		case <-timer:
			// we know they don't match at this point, but by using
			// ElementsMatch we get much better logging about the differences
			return assert.ElementsMatch(t, expected, s.Connections(containerID), "timed out waiting for network connections")
		case network := <-s.LiveConnections():
			if network.GetContainerId() != containerID {
				continue loop
			}

			to_find = funk.Filter(expected, func(x types.NetworkInfo) bool {
				return !s.HasConnection(containerID, x)
			}).([]types.NetworkInfo)

			if len(to_find) == 0 {
				return true
			}
		}
	}
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

	timer := time.After(timeout)
loop:
	for {
		select {
		case <-timer:
			assert.FailNowf(t, "timed out", "found %d connections (expected %d)", len(s.Connections(containerID)), n)
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

// checkIfConnectionsMatchExpected compares a list of expected and observed connection match exactly.
func (s *MockSensor) checkIfConnectionsMatchExpected(t *testing.T, connections []types.NetworkInfo, expected []types.NetworkInfo) bool {
	if len(connections) > len(expected) {
		return assert.ElementsMatch(t, expected, connections, "networking connections do not match")
	}

	if len(connections) == len(expected) {
		for i := range expected {
			if !expected[i].Equal(connections[i]) {
				return assert.ElementsMatch(t, expected, connections, "networking connections do not match")
			}
		}
		return true
	}
	return false
}

// getConnectionsAndCompare gets the connections for a container, sorts them, and compares with a set of expected
// connections. If asssertMismatch is true and the set of observed connections does not match the set of expected connections an assert is
// triggered. If assertMismatch is not set then just return if the observed and expected connections match.
func (s *MockSensor) getConnectionsAndCompare(t *testing.T, containerID string, assertMismatch bool, expected ...types.NetworkInfo) bool {
	connections := s.Connections(containerID)
	types.SortConnections(connections)
	success := s.checkIfConnectionsMatchExpected(t, connections, expected)
	if assertMismatch && !success {
		return assert.ElementsMatch(t, expected, connections, "networking connections do not match")
	}
	return success
}

// ExpectSameElementsConnections compares a list of expected connections to the observed connections. This comparison is done at the beginning, when a new
// connection arrives, and after a timeout period. The number of connections must match and the expected and observed connections must match, but the order
// does not matter.
func (s *MockSensor) ExpectSameElementsConnections(t *testing.T, containerID string, timeout time.Duration, expected ...types.NetworkInfo) bool {
	types.SortConnections(expected)

	success := s.getConnectionsAndCompare(t, containerID, false, expected...)
	if success {
		return true
	}

	timer := time.After(timeout)

	for {
		select {
		case <-timer:
			return s.getConnectionsAndCompare(t, containerID, true, expected...)
		case conn := <-s.LiveConnections():
			if conn.GetContainerId() != containerID {
				continue
			}
			success := s.getConnectionsAndCompare(t, containerID, false, expected...)
			if success {
				return true
			}
		}
	}
}

// ExpectEndpoints waits up to the timeout for the gRPC server to receive
// the list of expected Endpoints. It will first check to see if the endpoints
// have been received already, and then monitor the live feed of endpoints
// until timeout or until all the events have been received.
func (s *MockSensor) ExpectEndpoints(t *testing.T, containerID string, timeout time.Duration, expected ...types.EndpointInfo) bool {

	to_find := funk.Filter(expected, func(x types.EndpointInfo) bool {
		return !s.HasEndpoint(containerID, x)
	}).([]types.EndpointInfo)

	if len(to_find) == 0 {
		return true
	}

	timer := time.After(timeout)

loop:
	for {
		select {
		case <-timer:
			// we know they don't match at this point, but by using
			// ElementsMatch we get much better logging about the differences
			return assert.ElementsMatch(t, expected, s.Endpoints(containerID), "timed out waiting for endpoints")
		case network := <-s.LiveEndpoints():
			if network.GetContainerId() != containerID {
				continue loop
			}

			to_find = funk.Filter(expected, func(x types.EndpointInfo) bool {
				return !s.HasEndpoint(containerID, x)
			}).([]types.EndpointInfo)

			if len(to_find) == 0 {
				return true
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
	return s.waitEndpointsN(t, func() {
		assert.FailNowf(t, "timed out", "found %d endpoints (expected %d)", len(s.Endpoints(containerID)), n)
	}, containerID, timeout, n)
}

// WaitEndpointsN is a non-fatal version of ExpectEndpointsN. It waits for a given timeout
// until n Endpoints have been receieved. On timeout it returns false.
func (s *MockSensor) WaitEndpointsN(t *testing.T, containerID string, timeout time.Duration, n int) bool {
	return len(s.waitEndpointsN(t, func() {}, containerID, timeout, n)) == n
}

// waitEndpointsN is a helper function for waiting for a set number of endpoints.
// the timeoutFn function can be used to control error behaviour on timeout.
func (s *MockSensor) waitEndpointsN(t *testing.T, timeoutFn func(), containerID string, timeout time.Duration, n int) []types.EndpointInfo {
	seen := len(s.Endpoints(containerID))

	if seen == n {
		return s.Endpoints(containerID)
	} else if seen > n {
		assert.FailNow(t, "too many endpoints", "found %d endpoints (expected %d)", seen, n)
	}

	timer := time.After(timeout)
loop:
	for {
		select {
		case <-timer:
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
