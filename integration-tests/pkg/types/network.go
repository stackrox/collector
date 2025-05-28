package types

import (
	"fmt"
	"sort"

	sensorAPI "github.com/stackrox/rox/generated/internalapi/sensor"
	utils "github.com/stackrox/rox/pkg/net"
)

const (
	NilTimestamp = "<nil>"
)

type NetworkInfo struct {
	LocalAddress   string
	RemoteAddress  string
	Role           string
	SocketFamily   string
	CloseTimestamp string
}

type NetworkInfoBatch []NetworkInfo

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

func (n *NetworkInfo) String() string {
	return fmt.Sprintf("%s|%s|%s|%s|%s",
		n.LocalAddress,
		n.RemoteAddress,
		n.Role,
		n.SocketFamily,
		n.CloseTimestamp)
}

func (n *NetworkInfo) IsActive() bool {
	// no close timestamp means the connection is open, and active
	return n.CloseTimestamp == NilTimestamp
}

func (n *NetworkInfo) Equal(other NetworkInfo) bool {
	return n.LocalAddress == other.LocalAddress &&
		n.RemoteAddress == other.RemoteAddress &&
		n.Role == other.Role &&
		n.SocketFamily == other.SocketFamily &&
		n.IsActive() == other.IsActive()
}

func (n *NetworkInfo) Less(other NetworkInfo) bool {
	if n.LocalAddress != other.LocalAddress {
		return n.LocalAddress < other.LocalAddress
	}

	if n.RemoteAddress != other.RemoteAddress {
		return n.RemoteAddress < other.RemoteAddress
	}

	if n.Role != other.Role {
		return n.Role < other.Role
	}

	if n.SocketFamily != other.SocketFamily {
		return n.SocketFamily < other.SocketFamily
	}

	if n.IsActive() != other.IsActive() {
		return n.IsActive()
	}

	return false
}

func SortConnections(connections []NetworkInfo) {
	sort.Slice(connections, func(i, j int) bool { return connections[i].Less(connections[j]) })
}
