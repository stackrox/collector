package types

import (
	"sort"
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
