package types

import (
	"net"
	"sort"
	"time"

	timestamppb "google.golang.org/protobuf/types/known/timestamppb"

	sensorAPI "github.com/stackrox/rox/generated/internalapi/sensor"
)

const (
	NilTimestampStr = "<nil>"
)

var (
	NilTimestamp    = timestamppb.New(time.Date(1970, 1, 1, 0, 0, 0, 0, time.UTC))
	NotNilTimestamp = timestamppb.New(time.Date(2025, 1, 1, 0, 0, 0, 0, time.UTC))
)

type NetworkConnectionBatch []*sensorAPI.NetworkConnection

func IsActive(conn *sensorAPI.NetworkConnection) bool {
	// no close timestamp means the connection is open, and active
	return conn.GetCloseTimestamp() == nil
}

// The EqualVT method for NetworkAddress returns false if both of them are nil. That is not what
// we want, so replace nil addr with a default NetworkAddress.
func adjustNetworkAddressForComparison(addr *sensorAPI.NetworkAddress) *sensorAPI.NetworkAddress {
	if addr == nil {
		return CreateNetworkAddress("", "", 0)
	}

	return addr
}

// The EqualVT method for NetworkConnection returns false if both CloseTimestamps
// are nil. Same goes for LocalAddress and Remote Address. That is not the desired
// result. Also EqualVT returns false if the CloseTimestamp are different non-nil
// timestamps. We want the equal function to return true if neither of them are nil
// or both of them are nil. This function adjusts the fields so that the comparison
// works the way we want it to.
func adjustNetworkConnectionForComparison(conn sensorAPI.NetworkConnection) sensorAPI.NetworkConnection {
	conn.LocalAddress = adjustNetworkAddressForComparison(conn.LocalAddress)
	conn.RemoteAddress = adjustNetworkAddressForComparison(conn.RemoteAddress)

	if conn.CloseTimestamp == nil {
		conn.CloseTimestamp = NilTimestamp
	} else if conn.CloseTimestamp != nil {
		conn.CloseTimestamp = NotNilTimestamp
	}

	return conn
}

// EqualVT is not called directly because it returns false in cases that we don't want it to, for example
// when both CloseTimestamp are nil, or when they have different non-nil values.
func EqualNetworkConnection(conn1 sensorAPI.NetworkConnection, conn2 sensorAPI.NetworkConnection) bool {
	conn1 = adjustNetworkConnectionForComparison(conn1)
	conn2 = adjustNetworkConnectionForComparison(conn2)

	return conn1.EqualVT(&conn2)
}

func CompareBytes(b1 []byte, b2 []byte) int {
	if len(b1) != len(b2) {
		if len(b1) < len(b2) {
			return -1
		} else {
			return 1
		}
	}

	for i := range b1 {
		if b1[i] != b2[i] {
			if b1[i] < b2[i] {
				return -1
			} else {
				return 1
			}
		}
	}

	return 0
}

func EqualNetworkAddress(addr1 *sensorAPI.NetworkAddress, addr2 *sensorAPI.NetworkAddress) bool {
	ad1 := adjustNetworkAddressForComparison(addr1)
	ad2 := adjustNetworkAddressForComparison(addr2)

	return ad1.EqualVT(ad2)
}

func LessNetworkAddress(addr1 *sensorAPI.NetworkAddress, addr2 *sensorAPI.NetworkAddress) bool {
	comp := CompareBytes(addr1.GetAddressData(), addr2.GetAddressData())

	if comp != 0 {
		return comp < 0
	}

	comp = CompareBytes(addr1.GetIpNetwork(), addr2.GetIpNetwork())

	if comp != 0 {
		return comp < 0
	}

	return addr1.GetPort() < addr2.GetPort()
}

func LessNetworkConnection(conn1 *sensorAPI.NetworkConnection, conn2 *sensorAPI.NetworkConnection) bool {
	if !EqualNetworkAddress(conn1.LocalAddress, conn2.LocalAddress) {
		return LessNetworkAddress(conn1.GetLocalAddress(), conn2.GetLocalAddress())
	}

	if !EqualNetworkAddress(conn1.RemoteAddress, conn2.RemoteAddress) {
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
