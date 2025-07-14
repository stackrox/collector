package types

import (
	"bytes"
	"net"
	"sort"
	"time"

	"google.golang.org/protobuf/proto"
	timestamppb "google.golang.org/protobuf/types/known/timestamppb"

	sensorAPI "github.com/stackrox/rox/generated/internalapi/sensor"
)

const (
	NilTimestampStr = "<nil>"
)

var (
	NotNilTimestamp = timestamppb.New(time.Date(2025, 1, 1, 0, 0, 0, 0, time.UTC))
)

type NetworkConnectionBatch []*sensorAPI.NetworkConnection

func IsActive(conn *sensorAPI.NetworkConnection) bool {
	// no close timestamp means the connection is open, and active
	return conn.GetCloseTimestamp() == nil
}

// Equal is not called directly because it returns false when they have different non-nil values.
func EqualNetworkConnection(conn1 sensorAPI.NetworkConnection, conn2 sensorAPI.NetworkConnection) bool {
	// We don't care about the exact timestamp, only if it is nil or not nil
	adjustNetworkConnectionForComparison := func(conn *sensorAPI.NetworkConnection) *sensorAPI.NetworkConnection {
		if conn.CloseTimestamp != nil {
			conn = conn.CloneVT()
			conn.CloseTimestamp = NotNilTimestamp
		}

		return conn
	}

	copyConn1 := adjustNetworkConnectionForComparison(&conn1)
	copyConn2 := adjustNetworkConnectionForComparison(&conn2)

	return proto.Equal(copyConn1, copyConn2)
}

func EqualNetworkConnectionDontCompareCloseTimestamps(conn1 sensorAPI.NetworkConnection, conn2 sensorAPI.NetworkConnection) bool {
	conn1.CloseTimestamp = nil
	conn2.CloseTimestamp = nil

	return proto.Equal(&conn1, &conn2)
}

func LessNetworkAddress(addr1 *sensorAPI.NetworkAddress, addr2 *sensorAPI.NetworkAddress) bool {
	comp := bytes.Compare(addr1.GetAddressData(), addr2.GetAddressData())

	if comp != 0 {
		return comp < 0
	}

	comp = bytes.Compare(addr1.GetIpNetwork(), addr2.GetIpNetwork())

	if comp != 0 {
		return comp < 0
	}

	return addr1.GetPort() < addr2.GetPort()
}

func LessNetworkConnection(conn1 *sensorAPI.NetworkConnection, conn2 *sensorAPI.NetworkConnection) bool {
	if !proto.Equal(conn1.GetLocalAddress(), conn2.GetLocalAddress()) {
		return LessNetworkAddress(conn1.GetLocalAddress(), conn2.GetLocalAddress())
	}

	if !proto.Equal(conn1.GetRemoteAddress(), conn2.GetRemoteAddress()) {
		return LessNetworkAddress(conn1.GetRemoteAddress(), conn2.GetRemoteAddress())
	}

	if conn1.Role != conn2.Role {
		return conn1.Role < conn2.Role
	}

	if conn1.SocketFamily != conn2.SocketFamily {
		return conn1.SocketFamily < conn2.SocketFamily
	}

	if IsActive(conn1) != IsActive(conn2) {
		return IsActive(conn1)
	}

	return false
}

func stringToIPBytes(ipStr string) []byte {
	ip := net.ParseIP(ipStr)

	if ip == nil {
		return nil
	}

	return ip.To4()

}

func stringToIPNetworkBytes(ipStr string) []byte {
	ip := net.ParseIP(ipStr)

	if ip == nil {
		return nil
	}

	return append(ip.To4(), 32)
}

func CreateNetworkAddress(ipAddress string, ipNetwork string, port uint32) *sensorAPI.NetworkAddress {

	return &sensorAPI.NetworkAddress{
		AddressData: stringToIPBytes(ipAddress),
		IpNetwork:   stringToIPNetworkBytes(ipNetwork),
		Port:        port,
	}
}

func SortConnections(connections []*sensorAPI.NetworkConnection) {
	sort.Slice(connections, func(i, j int) bool { return LessNetworkConnection(connections[i], connections[j]) })
}
