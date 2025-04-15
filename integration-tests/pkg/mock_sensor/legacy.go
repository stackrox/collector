package mock_sensor

import (
	sensorAPI "github.com/stackrox/rox/generated/internalapi/sensor"
)

// This type is only used as a means to implement the legacy network service.
// Once the new network RPC is implemented, we can fully get rid of this.
type legacyNetworkServer struct {
	*MockSensor
}

func newLegacyServer(m *MockSensor) *legacyNetworkServer {
	return &legacyNetworkServer{
		MockSensor: m,
	}
}

// PushNetworkConnectionInfo conforms to the Sensor API. It is here that networking
// events (connections and endpoints) are handled and stored/sent to the relevant channel
func (l *legacyNetworkServer) PushNetworkConnectionInfo(stream sensorAPI.NetworkConnectionInfoService_PushNetworkConnectionInfoServer) error {
	for {
		signal, err := stream.Recv()
		if err != nil {
			return err
		}

		networkConnInfo := signal.GetInfo()
		l.pushNetworkConnectionInfo(networkConnInfo)
	}
}
