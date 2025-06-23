package types

import (
	"net"
	"sort"

	sensorAPI "github.com/stackrox/rox/generated/internalapi/sensor"
	utils "github.com/stackrox/rox/pkg/net"
)

const (
	NilTimestamp = "<nil>"
)

type NetworkConnectionBatch []*sensorAPI.NetworkConnection

// TranslateAddress is a helper function for converting binary representations
// of network addresses (in the signals) to usable forms for testing
func TranslateAddress(addr *sensorAPI.NetworkAddress) string {
	peerId := utils.NetworkPeerID{Port: uint16(addr.GetPort())}
	addressData := addr.GetAddressData()
	if len(addressData) > 0 {
		peerId.Address = utils.IPFromBytes(addressData)
		return peerId.String()
	}

	// If there is no address data, this is either the source address or
	// IpNetwork should be set and represent a CIDR block or external IP address.
	ipNetworkData := addr.GetIpNetwork()
	if len(ipNetworkData) == 0 {
		return peerId.String()
	}

	ipNetwork := utils.IPNetworkFromCIDRBytes(ipNetworkData)
	prefixLen := ipNetwork.PrefixLen()
	// If this is IPv4 and the prefix length is 32 or this is IPv6 and the prefix length
	// is 128 this is a regular IP address and not a CIDR block
	if (ipNetwork.Family() == utils.IPv4 && prefixLen == byte(32)) ||
		(ipNetwork.Family() == utils.IPv6 && prefixLen == byte(128)) {
		peerId.Address = ipNetwork.IP()
	} else {
		peerId.IPNetwork = ipNetwork
	}
	return peerId.String()
}

func IsActive(conn *sensorAPI.NetworkConnection) bool {
	// no close timestamp means the connection is open, and active
	return conn.GetCloseTimestamp() == nil
}

func Equal(conn1 *sensorAPI.NetworkConnection, conn2 *sensorAPI.NetworkConnection) bool {
	return EqualNetworkAddress(conn1.LocalAddress, conn2.LocalAddress) &&
		EqualNetworkAddress(conn1.RemoteAddress, conn2.RemoteAddress) &&
		conn1.Role == conn2.Role &&
		conn1.SocketFamily == conn2.SocketFamily &&
		IsActive(conn1) == IsActive(conn2)
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
	comp := CompareBytes(addr1.GetAddressData(), addr2.GetAddressData())

	if comp != 0 {
		return false
	}

	comp = CompareBytes(addr1.GetIpNetwork(), addr2.GetIpNetwork())

	if comp != 0 {
		return false
	}

	return addr1.GetPort() == addr2.GetPort()
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
